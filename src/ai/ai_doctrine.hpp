#pragma once

#include <string>
#include <vector>

namespace rts {

// What the AI wants to do. Loaded from data/ai_doctrines.json.
struct AIDoctrine {
    enum class UnitPreference { CHEAPEST, STRONGEST, MIXED };

    std::string key = "balanced";
    std::string name = "Balanced";
    // Zero means the scenario's own wave numbers.
    int first_wave = 0;
    int wave_growth = 0;
    int max_wave = 0;
    // Fighting units that never leave home.
    int home_guard = 0;
    // Enemies seen this close to the command post pull uncommitted units back to fight.
    float defence_radius = 1500.0f;
    UnitPreference unit_preference = UnitPreference::CHEAPEST;
    int scouts_wanted = 2;
    float first_recon_flight_seconds = 60.0f;
    float recon_flight_interval_seconds = 120.0f;
    int queue_depth = 2;
    // While nothing is known, keep the price of the next recon flight in the bank.
    bool save_for_recon_flight = true;
    // Waves wait until the enemy has been seen at least once.
    bool attack_only_known_enemy = false;
};

// How well the AI carries out its style. Never extra resources or vision.
struct AIDifficulty {
    std::string key = "normal";
    std::string name = "Normal";
    float decision_seconds = 1.0f;
    // A new contact is ignored until it has been in sight this long.
    float reaction_seconds = 0.0f;
    // Zero means no limit on unit orders per decision.
    int orders_per_decision = 0;
    // Units at or below this share of health head home. Zero never retreats.
    float retreat_health = 0.3f;
    float scout_retask_seconds = 60.0f;
};

struct AIDoctrineCatalog {
    std::string default_doctrine;
    std::string default_difficulty;
    std::vector<AIDoctrine> doctrines;
    std::vector<AIDifficulty> difficulties;
};

AIDoctrineCatalog parse_ai_doctrines(const std::string& json);
const AIDoctrineCatalog& get_ai_doctrines();
const AIDoctrine* find_ai_doctrine(const std::string& key);
const AIDifficulty* find_ai_difficulty(const std::string& key);
const AIDoctrine& default_ai_doctrine();
const AIDifficulty& default_ai_difficulty();

} // namespace rts
