#include "test_framework.hpp"
#include "simulation/terrain.hpp"
#include "simulation/simulation.hpp"
#include <fstream>

using namespace rts;
namespace {
int checks=0;
void verify(bool condition,const char* reason) { ++checks; if(!condition) throw std::runtime_error(reason); }
}

TEST(terrain_load_heightfield_file) {
    std::vector<float> test_data(320 * 320);
    for (size_t i = 0; i < test_data.size(); ++i) {
        test_data[i] = static_cast<float>(i);
    }
    
    std::string path = temp_file("test_heightmap.json");
    write_heightfield(path, 320, 320, test_data);
    
    Terrain terrain;
    terrain.load_from_file(path);
    
    verify(terrain.data().size() == 320 * 320, "terrain data size matches grid");
    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 320; ++x) {
            verify(std::abs(terrain.data()[y * 320 + x] - static_cast<float>(y * 320 + x)) < 0.001f, 
                   "terrain height value matches test data");
        }
    }
    std::cout << "terrain_load_binary_heightmap checks=" << checks << '\n';
}

TEST(terrain_height_at_coordinates) {
    std::vector<float> test_data(320 * 320);
    for (int y = 0; y < 320; ++y) {
        for (int x = 0; x < 320; ++x) {
            test_data[y * 320 + x] = static_cast<float>(y * 320 + x);
        }
    }
    
    std::string path = temp_file("test_heightmap.json");
    write_heightfield(path, 320, 320, test_data);
    
    Terrain terrain;
    terrain.load_from_file(path);
    
    // The 320 samples include both world edges, matching Godot's terrain mesh.
    verify(std::abs(terrain.height_at(-160.0f, -160.0f) - 0.0f) < 0.001f, "height at corner (-160,-160)");
    verify(std::abs(terrain.height_at(0.0f, 0.0f) - 51199.5f) < 0.001f, "height at center interpolates the rendered triangle");
    verify(std::abs(terrain.height_at(160.0f, 160.0f) - 102399.0f) < 0.001f, "height at corner (160,160)");
    std::cout << "terrain_height_at_coordinates checks=" << checks << '\n';
}

TEST(terrain_out_of_bounds_returns_zero) {
    std::vector<float> test_data(320 * 320, 50.0f);
    std::string path = temp_file("test_heightmap.json");
    write_heightfield(path, 320, 320, test_data);
    
    Terrain terrain;
    terrain.load_from_file(path);
    
    verify(std::abs(terrain.height_at(-200.0f, 0.0f)) < 0.001f, "out-of-bounds west");
    verify(std::abs(terrain.height_at(200.0f, 0.0f)) < 0.001f, "out-of-bounds east");
    verify(std::abs(terrain.height_at(0.0f, -200.0f)) < 0.001f, "out-of-bounds north");
    verify(std::abs(terrain.height_at(0.0f, 200.0f)) < 0.001f, "out-of-bounds south");
    std::cout << "terrain_out_of_bounds_returns_zero checks=" << checks << '\n';
}

TEST(terrain_ground_units_follow_heightfield) {
    std::vector<float> test_data(320 * 320, 12.0f);
    const std::string path = temp_file("test_heightmap_ground_units.json");
    write_heightfield(path, 320, 320, test_data);

    Simulation simulation;
    simulation.start();
    simulation.terrain().load_from_file(path);
    simulation.terrain().set_world_bounds(320.0f, 320.0f);
    const auto entity = simulation.create_unit_with_type(-100.0f, -100.0f,
        UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    verify(entity > 0, "ground unit created for heightfield collision test");
    auto* position = simulation.component_manager().get_component<Position>(static_cast<EntityId>(entity));
    verify(position && std::abs(position->z - 12.0f) < 0.001f, "ground unit spawns at terrain height");
    simulation.move_unit(static_cast<EntityId>(entity), -90.0f, -90.0f);
    simulation.update(50.0f);
    position = simulation.component_manager().get_component<Position>(static_cast<EntityId>(entity));
    verify(position && std::abs(position->z - 12.0f) < 0.001f, "ground unit retains terrain height after movement");
}

TEST(terrain_flatten_rect_levels_touching_points_only) {
    const std::string path = temp_file("flatten_terrain.json");
    std::vector<float> data(320 * 320, 0.0f);
    for (int row = 0; row < 320; ++row) {
        for (int column = 0; column < 320; ++column) {
            data[row * 320 + column] = static_cast<float>(column) * 10.0f;
        }
    }
    write_heightfield(path, 320, 320, data);
    Terrain terrain;
    terrain.set_world_bounds(40000.0f, 40000.0f);
    terrain.load_from_file(path);

    const uint32_t before = terrain.revision();
    verify(terrain.flatten_rect(0.0f, 0.0f, 30.0f, 30.0f, 500.0f), "loaded terrain can be levelled");
    verify(terrain.revision() == before + 1, "levelling advances the terrain revision");
    for (float x : {-30.0f, 0.0f, 30.0f}) {
        for (float y : {-30.0f, 0.0f, 30.0f}) {
            verify(std::abs(terrain.height_at(x, y) - 500.0f) < 0.01f, "levelled footprint is flat");
        }
    }
    verify(terrain.height_at(4000.0f, 0.0f) > 1900.0f, "ground away from the site keeps its height");
}

namespace {
// Rises east at 0.2, then 0.55, then 2.0: direct, earthworks and cliff ground.
std::string write_stepped_terrain() {
    const std::string path = temp_file("earthworks_terrain.json");
    std::vector<float> data(320 * 320, 0.0f);
    constexpr float spacing = 40000.0f / 319.0f;
    for (int row = 0; row < 320; ++row) {
        float height = 100.0f;
        for (int column = 0; column < 320; ++column) {
            data[row * 320 + column] = height;
            height += spacing * (column < 110 ? 0.2f : column < 210 ? 0.55f : 2.0f);
        }
    }
    write_heightfield(path, 320, 320, data);
    return path;
}
}

TEST(structure_sites_sort_ground_into_direct_earthworks_and_refused) {
    const std::string path = write_stepped_terrain();
    Simulation simulation;
    simulation.start();
    simulation.configure_world_size(40000.0f, 40000.0f);
    simulation.terrain().load_from_file(path);

    const auto direct = simulation.assess_structure_site(0, -12000.0f, 0.0f);
    verify(direct.buildable && !direct.earthworks, "gentle ground takes a structure directly");
    const auto graded = simulation.assess_structure_site(0, 0.0f, 0.0f);
    verify(graded.buildable && graded.earthworks && graded.material > 0.0f && graded.fuel > 0.0f && graded.seconds > 0.0f,
        "steep ground needs earthworks paid in Materials and fuel");
    const auto cliff = simulation.assess_structure_site(0, 12000.0f, 0.0f);
    verify(!cliff.buildable, "a cliff stays unbuildable");
    simulation.stop();
}

TEST(earthworks_flatten_the_terrain_and_spend_materials_and_fuel) {
    const std::string path = write_stepped_terrain();
    Simulation simulation;
    simulation.start();
    simulation.configure_world_size(40000.0f, 40000.0f);
    simulation.terrain().load_from_file(path);
    const EntityId line = simulation.create_faction_base(FactionId::ELITE_PRECISION, -12000.0f, 0.0f);
    verify(line != INVALID_ENTITY, "faction base exists");
    auto* funds = simulation.production_manager().tactical_resources(line);
    verify(funds != nullptr, "faction has resources");
    funds->materials.current = 5000.0f;
    funds->fuel.current = 5000.0f;

    const auto site = simulation.assess_structure_site(0, 0.0f, 0.0f);
    verify(site.earthworks, "fixture site needs earthworks");
    const float low_edge = simulation.terrain().height_at(-30.0f, 0.0f);
    const float high_edge = simulation.terrain().height_at(30.0f, 0.0f);
    verify(high_edge - low_edge > 10.0f, "fixture site starts sloped");

    funds->fuel.current = site.fuel - 1.0f;
    verify(!simulation.queue_structure(line, FactionId::ELITE_PRECISION, 0, 0.0f, 0.0f), "earthworks need enough fuel");
    funds->fuel.current = 5000.0f;
    verify(simulation.queue_structure(line, FactionId::ELITE_PRECISION, 0, 0.0f, 0.0f), "structure is accepted");
    const auto& queue = simulation.production_manager().production_lines().at(line).queue;
    verify(queue.size() == 2 && queue.front().structure_type == EARTHWORKS_JOB, "earthworks run before the structure");

    const uint32_t revision = simulation.terrain().revision();
    const int ticks = static_cast<int>(site.seconds * 20.0f) + 5;
    for (int tick = 0; tick < ticks; ++tick) simulation.update(50.0f);
    funds = simulation.production_manager().tactical_resources(line);
    verify(simulation.terrain().revision() == revision + 1, "earthworks change the terrain once");
    const float level = simulation.terrain().height_at(-30.0f, 0.0f);
    verify(std::abs(level - simulation.terrain().height_at(30.0f, 0.0f)) < 0.01f, "the site is level");
    verify(level > low_edge && level < high_edge, "the level sits between the old edges");
    verify(std::abs(funds->fuel.current - (5000.0f - site.fuel)) < 1.0f, "earthworks spend their fuel");
    verify(funds->materials.current < 5000.0f - site.material * 0.99f, "earthworks spend their Materials");
    verify(!simulation.assess_structure_site(0, 40.0f, 0.0f).earthworks, "levelled ground needs no more earthworks beside it");

    const auto ground = simulation.create_unit_with_type(0.0f, 20.0f, UnitType::ELITE_MAIN_BATTLE_TANK, FactionId::ELITE_PRECISION);
    verify(ground > 0, "unit fixture is created on the pad");
    const auto* position = simulation.component_manager().get_component<Position>(static_cast<EntityId>(ground));
    verify(position && std::abs(position->z - level) < 0.01f, "units stand on the flattened ground");
    simulation.stop();
}
