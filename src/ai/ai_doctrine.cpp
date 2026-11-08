#include "ai/ai_doctrine.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_set>

#include "data/json_parser.hpp"
#include "ecs/components/factions.hpp"

namespace rts {

namespace {

using data::JsonValue;

JsonValue require(const JsonValue& object, const char* field, JsonValue::Type type, const std::string& where) {
    auto value = object.get(field);
    if (!value || value->type() != type) {
        throw std::runtime_error("ai_doctrines.json: " + where + " needs " + field);
    }
    return *value;
}

float number(const JsonValue& object, const char* field, const std::string& where, float min, float max) {
    const double value = require(object, field, JsonValue::Type::Number, where).as_number();
    if (!std::isfinite(value) || value < min || value > max) {
        throw std::runtime_error("ai_doctrines.json: " + where + "." + field + " is out of range");
    }
    return static_cast<float>(value);
}

int whole(const JsonValue& object, const char* field, const std::string& where, int min, int max) {
    const float value = number(object, field, where, static_cast<float>(min), static_cast<float>(max));
    if (std::floor(value) != value) throw std::runtime_error("ai_doctrines.json: " + where + "." + field + " must be whole");
    return static_cast<int>(value);
}

bool flag(const JsonValue& object, const char* field, const std::string& where) {
    return require(object, field, JsonValue::Type::Bool, where).as_bool();
}

std::string text(const JsonValue& object, const char* field, const std::string& where) {
    std::string value = require(object, field, JsonValue::Type::String, where).as_string();
    if (value.empty()) throw std::runtime_error("ai_doctrines.json: " + where + "." + field + " is empty");
    return value;
}

AIDoctrine parse_doctrine(const JsonValue& entry) {
    AIDoctrine doctrine;
    doctrine.key = text(entry, "key", "doctrine");
    const std::string where = "doctrine " + doctrine.key;
    doctrine.name = text(entry, "name", where);
    text(entry, "description", where);
    doctrine.first_wave = whole(entry, "first_wave", where, 0, 500);
    doctrine.wave_growth = whole(entry, "wave_growth", where, 0, 500);
    doctrine.max_wave = whole(entry, "max_wave", where, 0, 2000);
    if ((doctrine.first_wave == 0) != (doctrine.max_wave == 0) || doctrine.max_wave < doctrine.first_wave) {
        throw std::runtime_error("ai_doctrines.json: " + where + " needs first_wave and max_wave both set, max at least first");
    }
    doctrine.home_guard = whole(entry, "home_guard", where, 0, 2000);
    doctrine.defence_radius = number(entry, "defence_radius", where, 0.0f, 20000.0f);
    const std::string preference = text(entry, "unit_preference", where);
    if (preference == "cheapest") doctrine.unit_preference = AIDoctrine::UnitPreference::CHEAPEST;
    else if (preference == "strongest") doctrine.unit_preference = AIDoctrine::UnitPreference::STRONGEST;
    else if (preference == "mixed") doctrine.unit_preference = AIDoctrine::UnitPreference::MIXED;
    else throw std::runtime_error("ai_doctrines.json: " + where + " has unknown unit_preference " + preference);
    doctrine.scouts_wanted = whole(entry, "scouts_wanted", where, 0, 20);
    doctrine.first_recon_flight_seconds = number(entry, "first_recon_flight_seconds", where, 0.0f, 3600.0f);
    doctrine.recon_flight_interval_seconds = number(entry, "recon_flight_interval_seconds", where, 1.0f, 3600.0f);
    doctrine.queue_depth = whole(entry, "queue_depth", where, 1, 20);
    doctrine.save_for_recon_flight = flag(entry, "save_for_recon_flight", where);
    doctrine.attack_only_known_enemy = flag(entry, "attack_only_known_enemy", where);
    return doctrine;
}

AIDifficulty parse_difficulty(const JsonValue& entry) {
    AIDifficulty difficulty;
    difficulty.key = text(entry, "key", "difficulty");
    const std::string where = "difficulty " + difficulty.key;
    difficulty.name = text(entry, "name", where);
    text(entry, "description", where);
    difficulty.decision_seconds = number(entry, "decision_seconds", where, 0.05f, 30.0f);
    difficulty.reaction_seconds = number(entry, "reaction_seconds", where, 0.0f, 120.0f);
    difficulty.orders_per_decision = whole(entry, "orders_per_decision", where, 0, 2000);
    difficulty.retreat_health = number(entry, "retreat_health", where, 0.0f, 0.95f);
    difficulty.scout_retask_seconds = number(entry, "scout_retask_seconds", where, 5.0f, 3600.0f);
    return difficulty;
}

} // namespace

AIDoctrineCatalog parse_ai_doctrines(const std::string& json) {
    const auto parsed = data::JsonParser::parse(json);
    if (!parsed || parsed->type() != JsonValue::Type::Object) throw std::runtime_error("ai_doctrines.json is not a JSON object");
    AIDoctrineCatalog catalog;
    catalog.default_doctrine = text(*parsed, "default_doctrine", "file");
    catalog.default_difficulty = text(*parsed, "default_difficulty", "file");
    std::unordered_set<std::string> keys;
    const JsonValue doctrines = require(*parsed, "doctrines", JsonValue::Type::Array, "file");
    for (const auto& entry : doctrines.as_array()) {
        catalog.doctrines.push_back(parse_doctrine(entry));
        if (!keys.insert("d:" + catalog.doctrines.back().key).second) throw std::runtime_error("ai_doctrines.json: duplicate doctrine key");
    }
    const JsonValue difficulties = require(*parsed, "difficulties", JsonValue::Type::Array, "file");
    for (const auto& entry : difficulties.as_array()) {
        catalog.difficulties.push_back(parse_difficulty(entry));
        if (!keys.insert("v:" + catalog.difficulties.back().key).second) throw std::runtime_error("ai_doctrines.json: duplicate difficulty key");
    }
    if (!keys.contains("d:" + catalog.default_doctrine) || !keys.contains("v:" + catalog.default_difficulty)) {
        throw std::runtime_error("ai_doctrines.json: the defaults must name a listed doctrine and difficulty");
    }
    return catalog;
}

const AIDoctrineCatalog& get_ai_doctrines() {
    static const AIDoctrineCatalog catalog = parse_ai_doctrines(read_game_data("ai_doctrines.json"));
    return catalog;
}

const AIDoctrine* find_ai_doctrine(const std::string& key) {
    for (const auto& doctrine : get_ai_doctrines().doctrines) {
        if (doctrine.key == key) return &doctrine;
    }
    return nullptr;
}

const AIDifficulty* find_ai_difficulty(const std::string& key) {
    for (const auto& difficulty : get_ai_doctrines().difficulties) {
        if (difficulty.key == key) return &difficulty;
    }
    return nullptr;
}

const AIDoctrine& default_ai_doctrine() { return *find_ai_doctrine(get_ai_doctrines().default_doctrine); }
const AIDifficulty& default_ai_difficulty() { return *find_ai_difficulty(get_ai_doctrines().default_difficulty); }

} // namespace rts
