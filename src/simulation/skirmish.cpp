#include "simulation/skirmish.hpp"
#include "data/json_parser.hpp"
#include "simulation/terrain.hpp"
#include <fstream>
#include <sstream>
#include <cmath>
#include <bit>
#include <iomanip>
#include <filesystem>
#include <openssl/sha.h>

namespace rts {
namespace {
using data::JsonValue;
std::string digest(const std::string& value) {
    unsigned char bytes[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(value.data()),value.size(),bytes);
    std::ostringstream out;
    for(auto b:bytes) out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
    return out.str();
}
std::string file_bytes(const std::string& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input) throw std::runtime_error("Required content file unavailable: "+path);
    std::ostringstream out; out << input.rdbuf(); return out.str();
}
const JsonValue& field(const JsonValue& value, const char* key) { return value.as_object().at(key); }
float number(const JsonValue& value) {
    if (value.type() != JsonValue::Type::Number || !std::isfinite(value.as_number())) throw std::runtime_error("Expected finite number");
    return static_cast<float>(value.as_number());
}
}
bool Skirmish::load(const std::string& path) {
    try {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Scenario file unavailable");
        std::stringstream bytes; bytes << input.rdbuf();
        auto parsed = data::JsonParser::parse(bytes.str());
        if (!parsed) throw std::runtime_error("Malformed scenario JSON");
        const auto& root = *parsed;
        // Godot's process working directory is the project directory, not
        // the repository root. Configure content lookup before any prototype
        // or research singleton is first read so the player route uses the
        // exact data that is hashed into the replay.
        // <repo>/godot/project/scenarios/<scenario>.json: four parents up is
        // the repository root where authored data/ is stored.
        const auto root_path=std::filesystem::path(path).parent_path().parent_path().parent_path().parent_path();
        const auto scenario_data_root=root_path/"data";
        if (std::getenv("RTS_DATA_ROOT") == nullptr) {
            ::setenv("RTS_DATA_ROOT", scenario_data_root.c_str(), 1);
        }
        if (number(field(root,"version")) != 1) throw std::runtime_error("Unsupported scenario version");
        const auto& theater = field(root,"theater");
        if (number(field(theater,"width")) != 320 || number(field(theater,"height")) != 320)
            throw std::runtime_error("Skirmish requires 320 by 320 theater");
        const auto& land = field(theater,"landmasses").as_array();
        if (land.size() != 2) throw std::runtime_error("Skirmish requires two landmasses");
        for (const auto& mass : land) {
            const auto& center = field(mass,"center").as_array();
            const auto& size = field(mass,"size").as_array();
            if (center.size()!=2 || size.size()!=2) throw std::runtime_error("Malformed landmass");
            for (int axis=0; axis<2; ++axis)
                if (number(size[axis])<=0 || std::abs(number(center[axis])) + number(size[axis])/2>160)
                    throw std::runtime_error("Landmass outside theater");
        }
        if (field(field(root,"victory"),"type").as_string() != "command_center")
            throw std::runtime_error("Unsupported victory rule");
        std::array<float,2> x{},y{};
        std::array<std::vector<UnitType>,2> roster;
        for (int faction=0; faction<2; ++faction) {
            const auto& side = field(root, faction==0 ? "player" : "ai");
            if (number(field(side,"faction_id")) != faction) throw std::runtime_error("Unsupported faction pairing");
            const auto& spawn = field(side,"spawn").as_array();
            if (spawn.size()!=2) throw std::runtime_error("Malformed spawn");
            x[faction]=number(spawn[0]); y[faction]=number(spawn[1]);
            const auto& center=field(land[faction],"center").as_array();
            const auto& size=field(land[faction],"size").as_array();
            if (std::abs(x[faction]-number(center[0]))+20>number(size[0])/2 ||
                std::abs(y[faction]-number(center[1]))+20>number(size[1])/2)
                throw std::runtime_error("Spawn needs twenty-unit land clearance");
            for (const auto& entry : field(side,"units").as_array()) {
                const float type=number(field(entry,"unit_type")), count=number(field(entry,"count"));
                if (type<0 || type>255 || std::floor(type)!=type || count<1 || count>100 || std::floor(count)!=count)
                    throw std::runtime_error("Invalid roster entry");
                const auto prototype=get_unit_prototypes().find(static_cast<UnitType>(type));
                if (prototype==get_unit_prototypes().end() || static_cast<int>(prototype->second.faction)!=faction)
                    throw std::runtime_error("Roster does not belong to faction");
                for (int i=0;i<count;++i) roster[faction].push_back(static_cast<UnitType>(type));
            }
            if (roster[faction].empty() || roster[faction].size()>100) throw std::runtime_error("Roster must contain 1 to 100 units");
        }
        const auto& support=field(root,"logistics");
        const float causeway=number(field(support,"causeway_half_width"));
        const float planes=number(field(support,"aircraft_count"));
        const float aircraft_fuel=number(field(support,"aircraft_fuel"));
        const auto& naval_spawn=field(support,"naval_spawn").as_array();
        if(aircraft_fuel<=0 || aircraft_fuel>120 || causeway<1 || causeway>10 || planes<1 || planes>4 || std::floor(planes)!=planes || naval_spawn.size()!=2)
            throw std::runtime_error("Invalid logistics setup");
        const float naval_x=number(naval_spawn[0]), naval_y=number(naval_spawn[1]);
        if(std::abs(naval_x)>=50 || std::abs(naval_y)<=causeway || std::abs(naval_y)>=150)
            throw std::runtime_error("Naval spawn must be in the sea lane");
        name_=field(root,"display_name").as_string();
        if (name_.empty() || x[0]>=x[1]) throw std::runtime_error("Invalid scenario identity or opposing spawns");
        scenario_path_=path;
        const auto data_root=std::getenv("RTS_DATA_ROOT") ? std::filesystem::path(std::getenv("RTS_DATA_ROOT")) : root_path/"data";
        content_hash_=digest(bytes.str()+file_bytes((data_root/"unit_faction_stats.json").string())+file_bytes((data_root/"research_projects.json").string()));
        
        // Load terrain heightmap if present
        if (auto terrain = root.get("terrain")) {
            if (terrain->type() == JsonValue::Type::Object) {
                if (auto heightmap = terrain->get("heightmap")) {
                    if (heightmap->type() == JsonValue::Type::Object) {
                        const auto& hm_obj = heightmap->as_object();
                        if (field(*heightmap, "data").type() == JsonValue::Type::String) {
                            simulation_.terrain().load_from_binary(field(*heightmap, "data").as_string().substr(3)); // Skip "res://"
                        }
                    }
                }
            }
        }
        
        // All input is checked before mutation. Runtime terrain is reset too.
        simulation_.start();
        for (int gy=0;gy<320;++gy) for(int gx=0;gx<320;++gx) {
            const float wx=gx-159.5f, wy=gy-159.5f;
            bool ground=std::abs(wy)<=causeway;
            for(const auto& mass:land) {
                const auto& center=field(mass,"center").as_array(); const auto& size=field(mass,"size").as_array();
                ground=ground || (std::abs(wx-number(center[0]))<=number(size[0])/2 && std::abs(wy-number(center[1]))<=number(size[1])/2);
            }
            simulation_.pathfinding().set_cell(gx,gy,ground);
            simulation_.naval_pathfinding().set_cell(gx,gy,!ground);
        }
        simulation_.enable_theater_water_rules();
        if (auto resources = root.get("resources"); resources && resources->type() == JsonValue::Type::Array) {
            for (const auto& resource : resources->as_array()) {
                const auto id = static_cast<EntityId>(number(field(resource, "id")));
                const auto position = field(resource, "position").as_array();
                if (position.size() != 2 || id == INVALID_ENTITY) throw std::runtime_error("Malformed resource node");
                const auto type_name = field(resource, "type").as_string();
                ResourceNode::Type type;
                if (type_name == "METAL") type = ResourceNode::Type::METAL;
                else if (type_name == "ENERGY") type = ResourceNode::Type::ENERGY;
                else if (type_name == "RESEARCH") type = ResourceNode::Type::RESEARCH;
                else throw std::runtime_error("Unsupported resource type");
                const float amount = number(field(resource, "amount"));
                if (amount <= 0) throw std::runtime_error("Resource amount must be positive");
                simulation_.production_manager().add_resource_node(id, ResourceNode{
                    number(position[0]), number(position[1]), amount, amount, type, false});
            }
        }
        for (int f=0;f<2;++f) {
            bases_[f]=simulation_.create_faction_base(static_cast<FactionId>(f),x[f],y[f]);
            for(size_t i=0;i<roster[f].size();++i) {
                auto id=simulation_.create_unit_with_type(x[f]+static_cast<int>(i%10)*3-13.5f,
                    y[f]+static_cast<int>(i/10)*3-13.5f,roster[f][i],static_cast<FactionId>(f));
                simulation_.stop_unit(id);
            }
        }
        Airbase airbase{}; airbase.x=x[0]; airbase.y=y[0]; airbase.runway_capacity=2;
        airbase.refuel_rate=20; airbase.rearm_rate=10; airbase.max_fuel=10000; airbase.current_fuel=10000;
        airbase.max_munitions=10000; airbase.current_munitions=10000; airbase.runway_usable=true;
        simulation_.logistics_manager().add_airbase(bases_[0],airbase);
        simulation_.logistics_manager().add_naval_base(bases_[0], x[0], y[0], 80.0f);
        for(int i=0;i<planes;++i) {
            auto id=simulation_.create_unit_with_type(x[0],y[0]+2*i,UnitType::ELITE_T1_FIGHTER,FactionId::ELITE_PRECISION);
            auto* aircraft=simulation_.component_manager().get_component<Aircraft>(id);
            aircraft->fuel=aircraft->max_fuel=aircraft_fuel;
        }
        const auto boat=simulation_.create_unit_with_type(naval_x,naval_y,UnitType::ELITE_PATROL_BOAT,FactionId::ELITE_PRECISION);
        simulation_.stop_unit(boat);
        simulation_.ai_manager().set_faction_id(FactionId::MASS_WARFARE);
        simulation_.ai_manager().set_objective(x[0],y[0]);
        checksums_.clear(); elapsed_=0; result_=-1; error_.clear();
        return true;
    } catch (const std::exception& error) { error_=error.what(); return false; }
}
void Skirmish::update(float milliseconds) {
    if (result_!=-1 || !std::isfinite(milliseconds) || milliseconds<=0) return;
    elapsed_=std::min(elapsed_+milliseconds,250.0f);
    while(elapsed_>=50 && result_==-1) {
        simulation_.update(50); elapsed_-=50;
        checksums_.push_back(checksum());
        const std::array<bool,2> alive{!simulation_.get_unit_is_dead(bases_[0]), !simulation_.get_unit_is_dead(bases_[1])};
        if(!alive[0] || !alive[1] || simulation_.simulation_tick() >= MAX_MATCH_TICKS) {
            result_=alive[0] == alive[1] ? 2 : alive[0] ? 0 : 1;
            simulation_.stop();
        }
    }
}
uint64_t Skirmish::checksum() {
    uint64_t hash=1469598103934665603ULL;
    auto add=[&](uint64_t value) { for(int i=0;i<8;++i) { hash^=(value>>(i*8))&255; hash*=1099511628211ULL; } };
    auto real=[&](float value) { add(std::bit_cast<uint32_t>(value)); };
    const auto state=simulation_.get_state(); add(state.tick_number);
    for(size_t i=0;i<state.entity_ids.size();++i) {
        add(state.entity_ids[i]); real(state.positions_x[i]); real(state.positions_y[i]);
        real(state.velocities_x[i]); real(state.velocities_y[i]); real(state.health_current[i]);
    }
    auto& production=simulation_.production_manager();
    for(int f=0;f<2;++f) {
        const auto funds=production.storages().find(bases_[f]);
        if(funds!=production.storages().end()) { real(funds->second.metal_storage); real(funds->second.energy_storage); real(funds->second.research_storage); }
        auto line=production.production_lines().find(bases_[f]);
        if(line!=production.production_lines().end()) {
            auto queue=line->second.queue; add(queue.size());
            while(!queue.empty()) { add(static_cast<int>(queue.front().unit_type)); real(queue.front().build_progress); queue.pop(); }
        }
        const auto& research=production.research(static_cast<FactionId>(f));
        for(uint32_t i=0;i<get_research_projects().size();++i) {
            const auto id=Simulation::research_id(i);
            auto done=research.completed_projects.find(id); add(done!=research.completed_projects.end() && done->second);
            add(!research.active_queue.empty() && research.active_queue.front()==id);
        }
        real(production.research_progress(static_cast<FactionId>(f)));
    }
    for(const auto& shot:simulation_.combat_manager().projectile_manager().projectiles()) {
        real(shot.x); real(shot.y); real(shot.vel_x); real(shot.vel_y); real(shot.time_alive); add(static_cast<int>(shot.faction_id));
    }
    return hash;
}
bool Skirmish::save_replay(const std::string& path) {
    if(result_<0) { error_="Only completed matches can be saved"; return false; }
    std::ostringstream body;
    body << "G08R1\n" << std::quoted(scenario_path_) << '\n' << content_hash_ << '\n';
    body << result_ << ' ' << checksums_.size() << ' ' << simulation_.command_log().size() << '\n';
    for(auto hash:checksums_) body << hash << '\n';
    for(const auto& cmd:simulation_.command_log()) body << cmd.tick_id << ' ' << cmd.entity_id << ' ' << static_cast<int>(cmd.player_id) << ' ' << static_cast<int>(cmd.cmd_type) << ' ' << cmd.target_x << ' ' << cmd.target_y << ' ' << cmd.extra << '\n';
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    std::ofstream output(path,std::ios::binary|std::ios::trunc);
    const auto payload=body.str(); output << digest(payload) << '\n' << payload;
    if(!output) { error_="Replay write failed"; return false; } return true;
}
bool Skirmish::replay(const std::string& path) {
    try {
        if(std::filesystem::file_size(path)>512*1024*1024) throw std::runtime_error("Replay exceeds size limit");
        const auto bytes=file_bytes(path);
        if(bytes.size()<65 || bytes[64]!='\n' || digest(bytes.substr(65))!=bytes.substr(0,64)) throw std::runtime_error("Replay integrity check failed");
        std::istringstream input(bytes.substr(65));
        std::string magic,scenario,content; size_t ticks, count; int result;
        if(!(input>>magic>>std::quoted(scenario)>>content>>result>>ticks>>count) || magic!="G08R1" || result<0 || result>2 || ticks==0 || ticks>MAX_MATCH_TICKS || count>ticks*MAX_COMMANDS_PER_TICK) throw std::runtime_error("Invalid replay header");
        std::vector<uint64_t> hashes(ticks);
        for(auto& hash:hashes) if(!(input>>hash)) throw std::runtime_error("Truncated replay checksums");
        std::vector<InputCommand> commands(count);
        uint32_t previous=0, per_tick=0;
        for(auto& cmd:commands) {
            uint64_t tick,entity,extra; int owner,type,x,y;
            if(!(input>>tick>>entity>>owner>>type>>x>>y>>extra) || tick<1 || tick>ticks || tick<previous || entity==0 || entity>=INVALID_ENTITY || extra>UINT32_MAX || owner<0 || owner>1 || type<0 || type>8 || x<INT16_MIN || x>INT16_MAX || y<INT16_MIN || y>INT16_MAX) throw std::runtime_error("Invalid replay command");
            per_tick=tick==previous?per_tick+1:1; previous=tick;
            if(per_tick>MAX_COMMANDS_PER_TICK) throw std::runtime_error("Replay tick capacity exceeded");
            cmd={static_cast<uint32_t>(tick),static_cast<EntityId>(entity),static_cast<uint8_t>(owner),static_cast<uint8_t>(type),static_cast<int16_t>(x),static_cast<int16_t>(y),static_cast<uint32_t>(extra)};
        }
        input>>std::ws; if(!input.eof()) throw std::runtime_error("Trailing replay data");
        // Verify using an isolated world; malformed or divergent playback never
        // mutates the match the player is reviewing.
        Simulation replay_simulation; Skirmish playback(replay_simulation);
        if(!playback.load(scenario) || playback.content_hash_!=content) throw std::runtime_error("Replay scenario/content mismatch");
        replay_simulation.enable_ai(false);
        size_t command=0;
        for(size_t tick=1;tick<=ticks;++tick) {
            while(command<commands.size() && commands[command].tick_id==tick) {
                if(!replay_simulation.command_manager().inject_local_command(commands[command++])) throw std::runtime_error("Replay queue overflow");
            }
            playback.update(50);
            if(playback.checksums_.size()!=tick || playback.checksums_.back()!=hashes[tick-1]) throw std::runtime_error("Replay state diverged at tick "+std::to_string(tick));
        }
        if(replay_simulation.command_log().size()!=commands.size()) throw std::runtime_error("Replay contains illegal commands");
        if(playback.result()!=result) throw std::runtime_error("Replay terminal result mismatch");
        error_.clear(); return true;
    } catch(const std::exception& error) { error_=error.what(); return false; }
}

}
