cmake_minimum_required(VERSION 3.20)
project(rts_simulation VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

find_package(OpenSSL REQUIRED)

# A clean checkout must not depend on an ignored, prebuilt vendor tree.  The
# commit is intentionally exact so the generated GDExtension bindings remain
# reproducible.  Godot 4.7.2 is the validated engine API for this project.
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API targeted by godot-cpp" FORCE)
set(GODOTCPP_TARGET "template_debug" CACHE STRING "godot-cpp template target" FORCE)
FetchContent_Declare(
    godot-cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG 9c8aeff0f58ad030f3d1030e8262de1322cd0ccd
    GIT_SHALLOW FALSE
)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(godot-cpp glm)

enable_testing()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# === Source files ===

set(SIMULATION_SOURCES
    src/ecs/entity.cpp
    src/ecs/component_manager.cpp
    src/spatial/spatial_grid.cpp
    src/pathfinding/pathfinding.cpp
    src/simulation/simulation.cpp
    src/simulation/skirmish.cpp
    src/simulation/terrain.cpp
    src/simulation/road_network.cpp
    src/simulation/off_road.cpp
    src/simulation/command_manager.cpp
    src/simulation/tick_manager.cpp
    src/logistics/logistics.cpp
    src/combat/projectile_manager.cpp
    src/combat/combat_manager.cpp
    src/production/production_manager.cpp
    src/network/buffer.cpp
    src/network/network_manager.cpp
    src/network/serializer.cpp
    src/network/portable_snapshot.cpp
    src/network/discovery.cpp
    src/network/tcp_transport.cpp
    src/replay/replay_writer.cpp
    src/replay/replay_reader.cpp
    src/render/mesh.cpp
    src/render/renderer.cpp
     src/ecs/components/factions.cpp
     src/ecs/components/territorial_control.cpp
    src/data/json_parser.cpp
    src/content_id/content_id.cpp
    src/mod/mod_manifest.cpp
     src/map/map_loader.cpp
      src/stats/stats_manager.cpp
      src/ai/ai_manager.cpp
)

# === Include directories ===

include_directories(${CMAKE_SOURCE_DIR}/src)

# === Simulation library ===

add_library(rts_simulation SHARED ${SIMULATION_SOURCES})
target_include_directories(rts_simulation PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rts_simulation PUBLIC OpenSSL::SSL OpenSSL::Crypto glm::glm)

# === GDExtension library ===

add_library(rts_gdextension SHARED gdextension/gd_extension.cpp)
target_include_directories(rts_gdextension PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rts_gdextension
    rts_simulation
    godot::cpp
)
set_target_properties(rts_gdextension PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)

# Keep the editor-facing binaries together so the extension's simulation
# dependency resolves independently of the shell's working directory.
add_custom_command(TARGET rts_gdextension POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_SOURCE_DIR}/godot/project/bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:rts_simulation>
        ${CMAKE_SOURCE_DIR}/godot/project/bin/$<TARGET_FILE_NAME:rts_simulation>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:rts_gdextension>
        ${CMAKE_SOURCE_DIR}/godot/project/bin/$<TARGET_FILE_NAME:rts_gdextension>
    COMMENT "Copying RTS native libraries into the Godot project"
)

# === Benchmark ===

add_executable(rts_benchmark benchmark/benchmark.cpp)
target_link_libraries(rts_benchmark rts_simulation)
target_include_directories(rts_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(rts_scale_benchmark benchmark/scale_benchmark.cpp)
target_link_libraries(rts_scale_benchmark rts_simulation)
target_include_directories(rts_scale_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(rts_combat_benchmark benchmark/combat_benchmark.cpp)
target_link_libraries(rts_combat_benchmark rts_simulation)
target_include_directories(rts_combat_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

# === Installation ===

install(TARGETS rts_simulation
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS rts_gdextension
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# === Tests ===

# Main test executable (does not include test_portable_snapshot.cpp due to static init issues)
add_executable(rts_tests
    tests/test_ecs.cpp
    tests/test_network.cpp
)
target_link_libraries(rts_tests rts_simulation)
target_include_directories(rts_tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME rts_tests COMMAND rts_tests)
set_tests_properties(rts_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Portable snapshot tests (separate executable due to static init issues)
add_executable(test_portable_snapshot
    tests/test_portable_snapshot.cpp
    src/network/portable_snapshot.cpp
)
target_link_libraries(test_portable_snapshot rts_simulation)
target_include_directories(test_portable_snapshot PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME test_portable_snapshot COMMAND test_portable_snapshot)
set_tests_properties(test_portable_snapshot PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Integration tests using test framework
add_executable(rts_integration_tests
    tests/test_ecs.cpp
    tests/test_logistics_ecs.cpp
    tests/test_logistics_scenario.cpp
    tests/test_logistics_visibility.cpp
    tests/test_ai.cpp
    tests/test_commands.cpp
    tests/test_skirmish.cpp
    tests/test_pathfinding.cpp
    tests/test_content_id.cpp
    tests/test_combat.cpp
    tests/test_health.cpp
    tests/test_mod_manifest.cpp
    tests/test_map_loader.cpp
    tests/test_replay.cpp
    tests/test_economy.cpp
    tests/test_terrain.cpp
    tests/test_territorial_control.cpp
    tests/test_stats.cpp
    tests/test_framework.hpp
)
target_link_libraries(rts_integration_tests rts_simulation)
target_include_directories(rts_integration_tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME rts_integration_tests COMMAND rts_integration_tests)
set_tests_properties(rts_integration_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Logistics benchmark
add_executable(rts_logistics_benchmark benchmark/logistics_benchmark.cpp)
target_link_libraries(rts_logistics_benchmark rts_simulation)
target_include_directories(rts_logistics_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(rts_skirmish_benchmark benchmark/skirmish_benchmark.cpp)
target_link_libraries(rts_skirmish_benchmark rts_simulation)
target_include_directories(rts_skirmish_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

# Empty or corrupt executables must not be mistaken for passing suites.
set_tests_properties(rts_tests rts_integration_tests PROPERTIES PASS_REGULAR_EXPRESSION "Results: [1-9][0-9]* passed, 0 failed")
set_tests_properties(test_portable_snapshot PROPERTIES PASS_REGULAR_EXPRESSION "All portable_snapshot tests passed")
