#include "ai/ai_manager.hpp"
#include "simulation/simulation.hpp"
#include "ecs/components/faction.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace rts {

namespace {
// Close enough that a scout driving between points misses nothing in between.
constexpr float kSectorMeters = 1000.0f;
// Ground not looked at for this long is worth another flight.
constexpr uint32_t kSectorStaleTicks = 20 * 180;
constexpr uint32_t kSweepLegTicks = 20 * 30;

uint32_t seconds_to_ticks(float seconds) { return static_cast<uint32_t>(std::lround(seconds * 20.0f)); }

bool is_recon_plane(const UnitPrototype& prototype) { return prototype.recon_loiter_seconds > 0.0f; }

// A unit that does nothing but recon, unlike a boat or engineer that also can.
bool is_scout(const UnitPrototype& prototype) {
    return !prototype.is_aircraft && !prototype.is_naval && prototype.capabilities.size() == 1 &&
           prototype.capabilities.front() == SeizureCapability::RECON;
}
} // namespace

AIManager::AIManager() { restore_default_doctrine(); }

// The built-in values match the file's defaults (a test holds them together),
// so this never reads data: a Simulation can exist before the data folder is
// configured, as it does when the native skirmish loads.
void AIManager::restore_default_doctrine() {
    set_doctrine(AIDoctrine{});
    set_difficulty(AIDifficulty{});
}

void AIManager::set_doctrine(const AIDoctrine& doctrine) {
    doctrine_ = doctrine;
    home_guard_.clear();
    apply_wave_plan();
}

void AIManager::set_difficulty(const AIDifficulty& difficulty) {
    difficulty_ = difficulty;
    decision_interval_ = difficulty.decision_seconds * 1000.0f;
}

// A style with its own wave numbers replaces the briefing's. Waves already
// sent keep counting; only the size of the next one changes.
void AIManager::apply_wave_plan() {
    const bool own = doctrine_.first_wave > 0;
    const int first = own ? doctrine_.first_wave : scenario_first_wave_;
    wave_growth_ = own ? doctrine_.wave_growth : scenario_wave_growth_;
    max_wave_ = own ? doctrine_.max_wave : scenario_max_wave_;
    if (first <= 0) return;
    next_wave_size_ = first;
    for (int wave = 0; wave < waves_launched_; ++wave) next_wave_size_ = std::min(max_wave_, next_wave_size_ + wave_growth_);
}

void AIManager::set_simulation(Simulation* sim) {
    reset();
    simulation_ = sim;
}

void AIManager::set_faction_id(FactionId faction_id) {
    if (faction_id != FactionId::ELITE_PRECISION &&
        faction_id != FactionId::MASS_WARFARE &&
        faction_id != FactionId::INDUSTRIAL_EXPERIMENTAL) return;
    reset();
    faction_id_ = faction_id;
}

void AIManager::update(float delta_ms) {
    if (!simulation_ || !std::isfinite(delta_ms) || delta_ms <= 0.0f) {
        return;
    }
    
    time_since_last_decision_ += delta_ms;
    
    if (time_since_last_decision_ >= decision_interval_) {
        time_since_last_decision_ = std::fmod(time_since_last_decision_, decision_interval_);
        update_visibility();
        update_sectors();
        gather_resources();
        produce_units();
        plan_expansion();
        update_scouts();
        update_waves();
        update_sweep();
        update_operational_plan();
        defend_base();
        
        if (!visible_units_.empty()) {
            attack_enemy();
        }
    }
}

void AIManager::reset() {
    visible_units_.clear();
    enemy_units_.clear();
    army_groups_.clear();
    focus_target_ = INVALID_ENTITY;
    expansion_target_ = INVALID_ENTITY;
    front_x_ = front_y_ = staging_x_ = staging_y_ = 0.0f;
    time_since_last_decision_ = 0.0f;
    objective_set_ = false;
    assault_active_ = false;
    next_wave_size_ = wave_growth_ = max_wave_ = waves_launched_ = 0;
    expansion_unit_ = INVALID_ENTITY;
    committed_.clear();
    sectors_.clear();
    scout_tasks_.clear();
    next_sweep_tick_ = next_flight_tick_ = 0;
    scenario_first_wave_ = scenario_wave_growth_ = scenario_max_wave_ = 0;
    home_guard_.clear();
    first_seen_.clear();
    order_cursor_ = mixed_cursor_ = 0;
    scouts_on_order_ = 0;
    arrived_scouts_.clear();
    recon_flights_ordered_ = 0;
    sites_surveyed_ = false;
    unknown_sites_.clear();
    enemy_sites_.clear();
}

void AIManager::configure_assault(float min_x, float min_y, float max_x, float max_y, int first_wave, int wave_growth, int max_wave) {
    if (!std::isfinite(min_x) || !std::isfinite(min_y) || !std::isfinite(max_x) || !std::isfinite(max_y) ||
        max_x <= min_x || max_y <= min_y || first_wave <= 0 || wave_growth < 0 || max_wave < first_wave) return;
    set_objective((min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f);
    area_min_x_ = min_x;
    area_min_y_ = min_y;
    area_max_x_ = max_x;
    area_max_y_ = max_y;
    sectors_.clear();
    scout_tasks_.clear();
    sweep_x_ = (min_x + max_x) * 0.5f;
    sweep_y_ = (min_y + max_y) * 0.5f;
    assault_active_ = true;
    // A first flight would otherwise leave on the opening tick, before the AI
    // has spent anything on the units that have to act on what it finds.
    next_flight_tick_ = (simulation_ ? simulation_->simulation_tick() : 0) + seconds_to_ticks(doctrine_.first_recon_flight_seconds);
    scenario_first_wave_ = first_wave;
    scenario_wave_growth_ = wave_growth;
    scenario_max_wave_ = max_wave;
    waves_launched_ = 0;
    apply_wave_plan();
    committed_.clear();
    home_guard_.clear();
}

bool AIManager::is_recon_asset(EntityId unit) const {
    const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
    if (!data) return false;
    const auto prototype = get_unit_prototypes().find(static_cast<UnitType>(data->unit_type));
    return prototype != get_unit_prototypes().end() && (is_scout(prototype->second) || is_recon_plane(prototype->second));
}

int AIManager::scout_count() const {
    if (!simulation_) return 0;
    int scouts = 0;
    for (EntityId unit : visible_units_) {
        const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
        const auto prototype = data ? get_unit_prototypes().find(static_cast<UnitType>(data->unit_type)) : get_unit_prototypes().end();
        if (prototype != get_unit_prototypes().end() && is_scout(prototype->second)) ++scouts;
    }
    return scouts;
}

void AIManager::update_sectors() {
    if (!assault_active_) return;
    if (sectors_.empty()) {
        for (float y = area_min_y_ + kSectorMeters * 0.5f; y < area_max_y_; y += kSectorMeters) {
            for (float x = area_min_x_ + kSectorMeters * 0.5f; x < area_max_x_; x += kSectorMeters) {
                if (simulation_->is_land_position(x, y)) sectors_.push_back({x, y});
            }
        }
        if (sectors_.empty()) sectors_.push_back({(area_min_x_ + area_max_x_) * 0.5f, (area_min_y_ + area_max_y_) * 0.5f});
    }
    const uint32_t tick = simulation_->simulation_tick();
    for (auto& sector : sectors_) {
        if (simulation_->can_see(faction_id_, sector.x, sector.y)) sector.last_seen_tick = tick;
    }
}

// The patch nobody has looked at for longest, nearest first, leaving out any
// another scout is already heading for.
size_t AIManager::stalest_sector(float x, float y, EntityId scout) const {
    size_t best = 0;
    uint32_t best_age = std::numeric_limits<uint32_t>::max();
    float best_distance = std::numeric_limits<float>::infinity();
    for (size_t index = 0; index < sectors_.size(); ++index) {
        const bool taken = std::any_of(scout_tasks_.begin(), scout_tasks_.end(), [&](const auto& task) {
            return task.first != scout && task.second.sector == index;
        });
        if (taken && sectors_.size() > scout_tasks_.size()) continue;
        const auto& sector = sectors_[index];
        const uint32_t age = std::max(sector.last_seen_tick, sector.last_tasked_tick);
        const float dx = sector.x - x, dy = sector.y - y;
        const float distance = dx * dx + dy * dy;
        if (age < best_age || (age == best_age && distance < best_distance)) {
            best = index;
            best_age = age;
            best_distance = distance;
        }
    }
    return best;
}

// A plane sees everything along its route in from the west edge, not only its
// destination, so a flight is scored on all the stale ground that route crosses.
// Equal routes go to the one nearest home, which favours nothing about the enemy.
size_t AIManager::plot_recon_flight(float view_range, float base_x, float base_y) {
    const uint32_t tick = simulation_->simulation_tick();
    const auto stale = [&](const Sector& sector) {
        const uint32_t looked = std::max(sector.last_seen_tick, sector.last_tasked_tick);
        return looked == 0 || tick - looked > kSectorStaleTicks;
    };
    const auto on_route = [&](const Sector& destination, const Sector& sector) {
        return std::abs(sector.y - destination.y) <= view_range && sector.x <= destination.x + view_range;
    };
    size_t best = 0;
    int best_score = -1;
    float best_distance = std::numeric_limits<float>::infinity();
    for (size_t index = 0; index < sectors_.size(); ++index) {
        int score = 0;
        for (const auto& sector : sectors_) {
            if (stale(sector) && on_route(sectors_[index], sector)) ++score;
        }
        const float dx = sectors_[index].x - base_x, dy = sectors_[index].y - base_y;
        const float distance = dx * dx + dy * dy;
        if (score > best_score || (score == best_score && distance < best_distance)) {
            best = index;
            best_score = score;
            best_distance = distance;
        }
    }
    // The next flight leaves before this one arrives, so claim its ground now.
    const Sector destination = sectors_[best];
    for (auto& sector : sectors_) {
        if (on_route(destination, sector)) sector.last_tasked_tick = tick;
    }
    return best;
}

void AIManager::update_scouts() {
    if (!assault_active_ || sectors_.empty()) return;
    const uint32_t tick = simulation_->simulation_tick();
    for (auto task = scout_tasks_.begin(); task != scout_tasks_.end();) {
        const bool lost = std::find(visible_units_.begin(), visible_units_.end(), task->first) == visible_units_.end();
        task = lost ? scout_tasks_.erase(task) : std::next(task);
    }
    for (EntityId unit : visible_units_) {
        const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
        if (!data || !is_recon_asset(unit) || simulation_->on_recon_flight(unit)) continue;
        const auto task = scout_tasks_.find(unit);
        const bool reached = task != scout_tasks_.end() && sectors_[task->second.sector].last_seen_tick > task->second.tasked_tick;
        const bool overdue = task != scout_tasks_.end() && tick - task->second.tasked_tick > seconds_to_ticks(difficulty_.scout_retask_seconds);
        if (task != scout_tasks_.end() && !reached && !overdue) continue;
        const size_t next = stalest_sector(simulation_->get_unit_x(unit), simulation_->get_unit_y(unit), unit);
        sectors_[next].last_tasked_tick = tick;
        scout_tasks_[unit] = {next, tick};
        simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE, sectors_[next].x, sectors_[next].y);
    }
}

// With no contact, a wave searches together rather than scattering.
void AIManager::update_sweep() {
    if (!assault_active_ || sectors_.empty() || committed_.empty()) return;
    const uint32_t tick = simulation_->simulation_tick();
    if (tick < next_sweep_tick_) return;
    float center_x = 0.0f, center_y = 0.0f;
    int counted = 0;
    for (EntityId unit : visible_units_) {
        if (!committed_.contains(unit)) continue;
        center_x += simulation_->get_unit_x(unit);
        center_y += simulation_->get_unit_y(unit);
        ++counted;
    }
    if (counted == 0) return;
    const size_t next = stalest_sector(center_x / counted, center_y / counted);
    sectors_[next].last_tasked_tick = tick;
    sweep_x_ = sectors_[next].x;
    sweep_y_ = sectors_[next].y;
    next_sweep_tick_ = tick + kSweepLegTicks;
}

bool AIManager::nearest_contact(float x, float y, float& contact_x, float& contact_y) const {
    float best = std::numeric_limits<float>::infinity();
    for (const auto& [id, sighting] : simulation_->last_known(faction_id_)) {
        (void)id;
        const float dx = sighting.x - x, dy = sighting.y - y;
        const float distance = dx * dx + dy * dy;
        if (distance < best) {
            best = distance;
            contact_x = sighting.x;
            contact_y = sighting.y;
        }
    }
    return std::isfinite(best);
}

void AIManager::update_waves() {
    if (!assault_active_) return;
    const auto base = simulation_->production_manager().faction_line(faction_id_);
    if (base == INVALID_ENTITY) return;
    // The home guard is filled first and refilled as it takes losses.
    for (auto guard = home_guard_.begin(); guard != home_guard_.end();) {
        const bool lost = !std::binary_search(visible_units_.begin(), visible_units_.end(), *guard);
        guard = lost ? home_guard_.erase(guard) : std::next(guard);
    }
    std::vector<EntityId> ready;
    for (EntityId unit : visible_units_) {
        if (unit == base || unit == expansion_unit_ || committed_.contains(unit) || home_guard_.contains(unit) || is_recon_asset(unit)) continue;
        const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
        if (!data || data->speed <= 0.0f || !simulation_->component_manager().get_component<Weapon>(unit)) continue;
        if (static_cast<int>(home_guard_.size()) < doctrine_.home_guard) home_guard_.insert(unit);
        else ready.push_back(unit);
    }
    if (static_cast<int>(ready.size()) < next_wave_size_) return;
    // A cautious style does not march out blind.
    if (doctrine_.attack_only_known_enemy && enemy_units_.empty() && simulation_->last_known(faction_id_).empty()) return;
    committed_.insert(ready.begin(), ready.end());
    ++waves_launched_;
    next_wave_size_ = std::min(max_wave_, next_wave_size_ + wave_growth_);
}

bool AIManager::home_threatened(EntityId base) const {
    const float radius = doctrine_.defence_radius;
    const float base_x = simulation_->get_unit_x(base);
    const float base_y = simulation_->get_unit_y(base);
    return std::any_of(enemy_units_.begin(), enemy_units_.end(), [&](EntityId enemy) {
        const float dx = simulation_->get_unit_x(enemy) - base_x;
        const float dy = simulation_->get_unit_y(enemy) - base_y;
        return dx * dx + dy * dy <= radius * radius;
    });
}

void AIManager::update_visibility() {
    visible_units_.clear();
    enemy_units_.clear();
    
    auto& components = simulation_->component_manager();
    const auto alive = [&components](EntityId id) {
        const auto* health = components.get_component<Health>(id);
        return health && !health->is_dead && health->current > 0.0f;
    };
    // The legacy visible_units API denotes owned, controllable units.
    for (EntityId id : simulation_->get_entity_list()) {
        const auto* faction = components.get_component<Faction>(id);
        if (faction && faction->faction_id == faction_id_ && alive(id)) {
            visible_units_.push_back(id);
        }
    }
    std::sort(visible_units_.begin(), visible_units_.end());

    // In a match the AI reads the same fog map as the player, sight lines included.
    if (simulation_->vision_active()) {
        enemy_units_ = simulation_->seen_enemies(faction_id_);
        apply_reaction_delay();
        return;
    }

    // Per-faction current vision. Global remembered Intelligence records have
    // no observer faction, so they cannot authorize access to enemy state.
    struct Sensor {
        float x, y, range_squared;
    };
    std::vector<Sensor> sensors;
    sensors.reserve(visible_units_.size());
    for (EntityId observer : visible_units_) {
        const auto* position = components.get_component<Position>(observer);
        const auto* unit = components.get_component<UnitData>(observer);
        if (!position || !unit || !std::isfinite(unit->view_range) ||
            unit->view_range <= 0.0f) continue;
        sensors.push_back({position->x, position->y, unit->view_range * unit->view_range});
    }
    // Each enemy stops at the first sensor that sees it, so a large battle does
    // not pay for every observer and enemy pair on a decision tick.
    for (EntityId candidate : simulation_->get_entity_list()) {
        const auto* faction = components.get_component<Faction>(candidate);
        const auto* position = components.get_component<Position>(candidate);
        if (!faction || faction->faction_id == faction_id_ || !position || !alive(candidate)) continue;
        for (const auto& sensor : sensors) {
            const float dx = position->x - sensor.x;
            const float dy = position->y - sensor.y;
            if (dx * dx + dy * dy <= sensor.range_squared) {
                enemy_units_.push_back(candidate);
                break;
            }
        }
    }
    std::sort(enemy_units_.begin(), enemy_units_.end());
    apply_reaction_delay();
}

// A slower commander needs a moment to take in a new contact. Anything that
// drops out of sight starts the clock again when it reappears.
void AIManager::apply_reaction_delay() {
    const uint32_t tick = simulation_->simulation_tick();
    for (auto seen = first_seen_.begin(); seen != first_seen_.end();) {
        const bool gone = !std::binary_search(enemy_units_.begin(), enemy_units_.end(), seen->first);
        seen = gone ? first_seen_.erase(seen) : std::next(seen);
    }
    for (EntityId enemy : enemy_units_) first_seen_.try_emplace(enemy, tick);
    const uint32_t delay = seconds_to_ticks(difficulty_.reaction_seconds);
    if (delay == 0) return;
    std::erase_if(enemy_units_, [&](EntityId enemy) { return tick - first_seen_.at(enemy) < delay; });
}

std::vector<EntityId> AIManager::get_visible_units() const {
    return visible_units_;
}

std::vector<EntityId> AIManager::get_enemy_units() const {
    return enemy_units_;
}

void AIManager::set_objective(float x, float y) {
    if (!std::isfinite(x) || !std::isfinite(y)) return;
    objective_x_ = x; objective_y_ = y; objective_set_ = true;
}

void AIManager::gather_resources() {
    // Owned extractors run in the shared fixed-tick production phase.
}

void AIManager::produce_units() {
    if (!simulation_) return;
    auto& production = simulation_->production_manager();
    const auto base = production.faction_line(faction_id_);
    if (base == INVALID_ENTITY) return;
    if (assault_active_) {
        produce_for_assault(base);
        return;
    }
    // Recon units belong to the assault planner; this path only buys fighters.
    std::vector<UnitType> candidates;
    for (const auto& [type, prototype] : get_unit_prototypes())
        if (!is_scout(prototype) && !is_recon_plane(prototype) && production.can_queue_unit(base, faction_id_, type))
            candidates.push_back(type);
    std::sort(candidates.begin(), candidates.end(), [](UnitType a, UnitType b) {
        const auto& prototypes = get_unit_prototypes();
        const float ca = prototypes.at(a).material_cost, cb = prototypes.at(b).material_cost;
        return ca != cb ? ca < cb : a < b;
    });
    if (!candidates.empty()) simulation_->issue_build_commands({base}, faction_id_, 0, 0, static_cast<int>(candidates.front()));
}

// Everything here is an ordinary order through the production line, at the
// ordinary price. Scouts first, then a recon flight while nothing is known, and
// the fighting line with what is left.
void AIManager::produce_for_assault(EntityId base) {
    auto& production = simulation_->production_manager();
    const uint32_t tick = simulation_->simulation_tick();
    const float base_x = simulation_->get_unit_x(base);
    const float base_y = simulation_->get_unit_y(base);
    std::vector<UnitType> fighters;
    std::optional<UnitType> scout, plane;
    float flight_price = 0.0f;
    float flight_view = 0.0f;
    for (const auto& [type, prototype] : get_unit_prototypes()) {
        if (prototype.faction != faction_id_) continue;
        if (is_recon_plane(prototype)) {
            flight_price = prototype.material_cost + prototype.operational_material;
            flight_view = prototype.view_range;
        }
        if (!production.can_queue_unit(base, faction_id_, type)) continue;
        if (is_scout(prototype)) scout = type;
        else if (is_recon_plane(prototype)) plane = type;
        else if (!prototype.is_aircraft && !prototype.is_naval && !prototype.construction_unit) fighters.push_back(type);
    }
    // Fighters wait while scouts are short, or a scout sits behind them in the queue.
    for (EntityId unit : visible_units_) {
        const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
        const auto prototype = data ? get_unit_prototypes().find(static_cast<UnitType>(data->unit_type)) : get_unit_prototypes().end();
        if (prototype != get_unit_prototypes().end() && is_scout(prototype->second) && arrived_scouts_.insert(unit).second) {
            scouts_on_order_ = std::max(0, scouts_on_order_ - 1);
        }
    }
    if (scout && scout_count() + scouts_on_order_ < doctrine_.scouts_wanted) {
        simulation_->issue_build_commands({base}, faction_id_, base_x, base_y, static_cast<int>(*scout));
        ++scouts_on_order_;
        return;
    }
    const bool enemy_unknown = simulation_->last_known(faction_id_).empty();
    if (plane && !sectors_.empty() && enemy_unknown && tick >= next_flight_tick_) {
        const size_t target = plot_recon_flight(flight_view, base_x, base_y);
        simulation_->issue_build_commands({base}, faction_id_, sectors_[target].x, sectors_[target].y, static_cast<int>(*plane));
        ++recon_flights_ordered_;
        next_flight_tick_ = tick + seconds_to_ticks(doctrine_.recon_flight_interval_seconds);
        return;
    }
    std::sort(fighters.begin(), fighters.end(), [](UnitType a, UnitType b) {
        const auto& prototypes = get_unit_prototypes();
        const float ca = prototypes.at(a).material_cost, cb = prototypes.at(b).material_cost;
        return ca != cb ? ca < cb : a < b;
    });
    // The line builds one job at a time, so a short queue loses no output and
    // leaves room for a scout or flight that is suddenly needed.
    const auto line = production.production_lines().find(base);
    if (line == production.production_lines().end() || fighters.empty() ||
        line->second.queue.size() >= static_cast<size_t>(doctrine_.queue_depth)) return;
    UnitType choice = fighters.front();
    switch (doctrine_.unit_preference) {
        case AIDoctrine::UnitPreference::CHEAPEST: break;
        case AIDoctrine::UnitPreference::STRONGEST: choice = fighters.back(); break;
        case AIDoctrine::UnitPreference::MIXED: choice = fighters[mixed_cursor_ % fighters.size()]; break;
    }
    // Tanks are no use without something to aim them at, so while the enemy is
    // unknown the price of the next flight stays in the bank.
    const auto storage = production.storages().find(line->second.storage_id);
    const float price = get_unit_prototypes().at(choice).material_cost;
    if (doctrine_.save_for_recon_flight && enemy_unknown && storage != production.storages().end() &&
        storage->second.tactical.materials.current - price < flight_price) return;
    if (simulation_->issue_build_commands({base}, faction_id_, base_x, base_y, static_cast<int>(choice)) > 0) ++mixed_cursor_;
}

void AIManager::plan_expansion() {
    if (!simulation_) return;
    const auto base = simulation_->production_manager().faction_line(faction_id_);
    if (base == INVALID_ENTITY) return;
    std::vector<EntityId> mobile;
    for (EntityId unit : visible_units_) {
        if (unit == base || is_recon_asset(unit)) continue;
        const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
        if (data && data->speed > 0.0f) mobile.push_back(unit);
    }
    if (mobile.empty()) return;
    std::sort(mobile.begin(), mobile.end());
    expansion_unit_ = mobile.front();
    const auto own_storage = simulation_->production_manager().faction_storage(faction_id_);
    std::vector<EntityId> nodes;
    for (const auto& [id, node] : simulation_->production_manager().resource_nodes()) {
        if (!node.depleted && node.amount > 0.0f) nodes.push_back(id);
    }
    std::sort(nodes.begin(), nodes.end());
    EntityId selected = INVALID_ENTITY;
    float selected_amount = -1.0f;
    for (EntityId id : nodes) {
        bool own = false, foreign = false;
        for (const auto& [extractor_id, extractor] : simulation_->production_manager().extractors()) {
            (void)extractor_id;
            if (extractor.resource_node_id != id || (!extractor.active && extractor.storage_id == INVALID_ENTITY)) continue;
            (assault_active_ && extractor.storage_id != own_storage ? foreign : own) = true;
        }
        const auto& node = simulation_->production_manager().resource_nodes().at(id);
        if (assault_active_) {
            // Sites held by the enemy at the start are as hidden from the AI as
            // its depots are from the player. After that it knows only what it
            // has seen, so it can still walk up to a site the enemy took.
            if (!sites_surveyed_ && foreign) unknown_sites_.insert(id);
            if (simulation_->can_see(faction_id_, node.x, node.y)) {
                unknown_sites_.erase(id);
                if (foreign) enemy_sites_.insert(id);
                else enemy_sites_.erase(id);
            }
            if (unknown_sites_.contains(id) || enemy_sites_.contains(id)) continue;
        }
        if (!own && (node.amount > selected_amount || (node.amount == selected_amount && id < selected))) {
            selected = id;
            selected_amount = node.amount;
        }
    }
    sites_surveyed_ = true;
    expansion_target_ = selected;
    if (selected != INVALID_ENTITY) {
        const auto& node = simulation_->production_manager().resource_nodes().at(selected);
        simulation_->issue_harvest_commands({mobile.front()}, faction_id_, node.x, node.y);
    }
}

void AIManager::attack_enemy() {
    if (!simulation_) return;
    const auto base = simulation_->production_manager().faction_line(faction_id_);
    // Scenario objectives are public map positions, never hidden enemy queries.
    if (base == INVALID_ENTITY || simulation_->get_unit_is_dead(base)) return;
    update_visibility(); // Refresh once per decision, not once per unit/target pair.
    focus_target_ = select_focus_target(base);
    const bool defending_home = assault_active_ && home_threatened(base);
    // A slower commander gets through only so many units each decision. The
    // starting point turns over so every unit is reached in time.
    const size_t budget = difficulty_.orders_per_decision > 0 ? static_cast<size_t>(difficulty_.orders_per_decision)
                                                              : std::numeric_limits<size_t>::max();
    std::vector<EntityId> units = visible_units_;
    if (!units.empty()) std::rotate(units.begin(), units.begin() + static_cast<std::ptrdiff_t>(order_cursor_ % units.size()), units.end());
    size_t ordered = 0, considered = 0;
    for (auto unit : units) {
        if (ordered >= budget) break;
        ++considered;
        if (unit == base || is_recon_asset(unit)) continue;
        if (assault_active_ && (unit == expansion_unit_ || (!defending_home && !committed_.contains(unit)))) continue;
        const auto* position = simulation_->component_manager().get_component<Position>(unit);
        const auto* weapon = simulation_->component_manager().get_component<Weapon>(unit);
        const auto* sensor = simulation_->component_manager().get_component<UnitData>(unit);
        if (!position || !weapon || !sensor || sensor->speed <= 0) continue;
        ++ordered;
        const auto* health = simulation_->component_manager().get_component<Health>(unit);
        if (difficulty_.retreat_health > 0.0f && health && health->max > 0.0f && health->current / health->max <= difficulty_.retreat_health) {
            simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE,
                                        simulation_->get_unit_x(base), simulation_->get_unit_y(base));
            continue;
        }
        if (focus_target_ != INVALID_ENTITY) {
            const float dx = simulation_->get_unit_x(focus_target_) - position->x;
            const float dy = simulation_->get_unit_y(focus_target_) - position->y;
            const float distance = dx * dx + dy * dy;
            const float range = std::min(weapon->range, sensor->view_range) * 0.8f;
            if (distance > range * range) {
                simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE,
                                            simulation_->get_unit_x(focus_target_), simulation_->get_unit_y(focus_target_));
            } else {
                simulation_->issue_stop_commands({unit}, faction_id_);
                simulation_->issue_attack_commands({unit}, faction_id_, focus_target_);
            }
            continue;
        }
        EntityId target = INVALID_ENTITY;
        float best = std::numeric_limits<float>::infinity();
        bool defending = false;
        const float base_x = simulation_->get_unit_x(base), base_y = simulation_->get_unit_y(base);
        for (auto enemy : enemy_units_) {
            const float dx = simulation_->get_unit_x(enemy) - position->x;
            const float dy = simulation_->get_unit_y(enemy) - position->y;
            const float distance = dx*dx + dy*dy;
            const float bx=simulation_->get_unit_x(enemy)-base_x, by=simulation_->get_unit_y(enemy)-base_y;
            const bool threat=bx*bx+by*by <= 40*40;
            if ((threat && !defending) || (threat == defending && distance < best)) {
                best = distance; target = enemy; defending = threat;
            }
        }
        if (target != INVALID_ENTITY) {
            const float range = std::min(weapon->range, sensor->view_range) * 0.8f;
            if (best > range*range) simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE,
                simulation_->get_unit_x(target), simulation_->get_unit_y(target));
            else {
                simulation_->issue_stop_commands({unit}, faction_id_);
                simulation_->issue_attack_commands({unit}, faction_id_, target);
            }
        } else if (assault_active_) {
            // Nothing in sight: head for the last place the enemy was seen, or
            // keep searching. Never a hidden unit's real position.
            float x = sweep_x_, y = sweep_y_;
            nearest_contact(position->x, position->y, x, y);
            simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE, x, y);
        } else if (objective_set_) {
            // Search a public, bounded objective perimeter when no enemy is
            // currently visible. This does not read hidden unit coordinates.
            const auto corner = (simulation_->simulation_tick()/200 + unit%4)%4;
            const float x = objective_x_ + (corner<2 ? -15.0f : 15.0f);
            const float y = objective_y_ + (corner%2 ? -15.0f : 15.0f);
            simulation_->issue_commands({unit}, faction_id_, CommandType::MOVE, x, y);
        }
    }
    order_cursor_ = units.empty() ? 0 : (order_cursor_ + considered) % units.size();
}

EntityId AIManager::select_focus_target(EntityId base) const {
    EntityId selected = INVALID_ENTITY;
    float selected_distance = std::numeric_limits<float>::infinity();
    bool selected_is_base_threat = false;
    const float base_x = simulation_->get_unit_x(base), base_y = simulation_->get_unit_y(base);
    for (EntityId enemy : enemy_units_) {
        const float dx = simulation_->get_unit_x(enemy) - base_x;
        const float dy = simulation_->get_unit_y(enemy) - base_y;
        const float distance = dx * dx + dy * dy;
        const bool is_base_threat = distance <= 40.0f * 40.0f;
        if ((is_base_threat && !selected_is_base_threat) ||
            (is_base_threat == selected_is_base_threat &&
             (distance < selected_distance || (distance == selected_distance && enemy < selected)))) {
            selected = enemy;
            selected_distance = distance;
            selected_is_base_threat = is_base_threat;
        }
    }
    return selected;
}

void AIManager::update_operational_plan() {
    army_groups_.clear();
    const auto base = simulation_->production_manager().faction_line(faction_id_);
    if (base == INVALID_ENTITY) return;
    std::vector<EntityId> mobile;
    for (EntityId unit : visible_units_) {
        if (unit == base) continue;
        const auto* data = simulation_->component_manager().get_component<UnitData>(unit);
        if (data && data->speed > 0.0f) mobile.push_back(unit);
    }
    std::sort(mobile.begin(), mobile.end());
    constexpr size_t group_size = 8;
    for (size_t offset = 0; offset < mobile.size(); offset += group_size) {
        army_groups_.emplace_back(mobile.begin() + static_cast<std::ptrdiff_t>(offset),
                                  mobile.begin() + static_cast<std::ptrdiff_t>(std::min(mobile.size(), offset + group_size)));
    }
    if (!enemy_units_.empty()) {
        const EntityId target = select_focus_target(base);
        front_x_ = simulation_->get_unit_x(target);
        front_y_ = simulation_->get_unit_y(target);
    } else if (objective_set_) {
        front_x_ = objective_x_;
        front_y_ = objective_y_;
    } else {
        front_x_ = simulation_->get_unit_x(base);
        front_y_ = simulation_->get_unit_y(base);
    }
    staging_x_ = (simulation_->get_unit_x(base) + front_x_) * 0.5f;
    staging_y_ = (simulation_->get_unit_y(base) + front_y_) * 0.5f;
}

void AIManager::defend_base() {
    // Visible nearby threats take precedence over the public advance objective
    // in attack_enemy; no separate stop order overwrites those responses.
}

} // namespace rts
