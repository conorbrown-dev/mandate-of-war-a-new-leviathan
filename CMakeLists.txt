cmake_minimum_required(VERSION 3.20)
project(rts_simulation VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

# A clean checkout must not depend on an ignored, prebuilt vendor tree.  The
# commit is intentionally exact so the generated GDExtension bindings remain
# reproducible.  Godot 4.7.2 is the validated engine API for this project.
set(GODOTCPP_API_VERSION "4.7" CACHE STRING "Godot API targeted by godot-cpp" FORCE)
set(GODOTCPP_TARGET "template_debug" CACHE STRING "godot-cpp template target" FORCE)
# The simulation reports bad content by throwing, so exceptions must stay on
# when MSVC compiles the Windows bridge against the same standard library.
if(MSVC)
    set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "Keep C++ exceptions enabled" FORCE)
endif()
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

if(MSVC)
    add_compile_definitions(NOMINMAX _USE_MATH_DEFINES _CRT_SECURE_NO_WARNINGS)
    add_compile_options(/utf-8 /bigobj)
endif()

# === Source files ===

set(SIMULATION_SOURCES
    src/ecs/entity.cpp
    src/ecs/component_manager.cpp
    src/spatial/spatial_grid.cpp
    src/pathfinding/pathfinding.cpp
    src/util/environment.cpp
    src/simulation/simulation.cpp
    src/simulation/skirmish.cpp
    src/simulation/terrain.cpp
    src/simulation/road_network.cpp
    src/simulation/off_road.cpp
    src/simulation/command_manager.cpp
    src/simulation/control_point_battle.cpp
    src/simulation/reinforcement_delivery.cpp
    src/simulation/tick_manager.cpp
    src/logistics/logistics.cpp
     src/combat/projectile_manager.cpp
     src/combat/combat_manager.cpp
     src/production/production_manager.cpp
     src/production/requisition_manager.cpp
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
      src/ai/ai_doctrine.cpp
      src/simulation/match_record.cpp
)

# === Include directories ===

include_directories(${CMAKE_SOURCE_DIR}/src)

# === Simulation library ===

# Windows links the simulation into the extension DLL: nothing here marks
# symbols for export, and one DLL has no dependency to resolve at load time.
if(WIN32)
    add_library(rts_simulation STATIC ${SIMULATION_SOURCES})
    target_link_libraries(rts_simulation PUBLIC ws2_32)
else()
    add_library(rts_simulation SHARED ${SIMULATION_SOURCES})
endif()
target_include_directories(rts_simulation PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(rts_simulation PUBLIC glm::glm)

# === GDExtension library ===

add_library(rts_gdextension SHARED gdextension/gd_extension.cpp)
target_include_directories(rts_gdextension PUBLIC
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(rts_gdextension
    rts_simulation
    godot::cpp
)
# The extension finds the simulation library beside itself in godot/project/bin.
if(APPLE)
    set(RTS_LOADER_RPATH "@loader_path")
else()
    set(RTS_LOADER_RPATH "$ORIGIN")
endif()
set_target_properties(rts_gdextension PROPERTIES
    BUILD_RPATH "${RTS_LOADER_RPATH}"
    INSTALL_RPATH "${RTS_LOADER_RPATH}"
)

# Keep the editor-facing binaries together so the extension's simulation
# dependency resolves independently of the shell's working directory.
add_custom_command(TARGET rts_gdextension POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        ${CMAKE_SOURCE_DIR}/godot/project/bin
    COMMAND $<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_COMMAND}> $<$<NOT:$<PLATFORM_ID:Windows>>:-E> $<$<NOT:$<PLATFORM_ID:Windows>>:copy_if_different>
        $<$<NOT:$<PLATFORM_ID:Windows>>:$<TARGET_FILE:rts_simulation>>
        $<$<NOT:$<PLATFORM_ID:Windows>>:${CMAKE_SOURCE_DIR}/godot/project/bin/$<TARGET_FILE_NAME:rts_simulation>>
    COMMAND_EXPAND_LISTS
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:rts_gdextension>
        ${CMAKE_SOURCE_DIR}/godot/project/bin/$<TARGET_FILE_NAME:rts_gdextension>
    COMMENT "Copying RTS native libraries into the Godot project"
)

# === Benchmark ===

# The integration diagnostic reads Linux process memory and loads the library
# by path, so it is Linux-only.
if(NOT WIN32)
    add_executable(rts_benchmark benchmark/benchmark.cpp)
    target_link_libraries(rts_benchmark rts_simulation)
    target_include_directories(rts_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)
endif()

# The accepted movement/formation harness (tools/validate.py simulation_scale_1000).
add_executable(rts_intel_benchmark benchmark/intel_benchmark.cpp)
target_link_libraries(rts_intel_benchmark rts_simulation)
target_include_directories(rts_intel_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

add_executable(rts_scale_benchmark benchmark/scale_benchmark.cpp)
target_link_libraries(rts_scale_benchmark rts_simulation)
target_include_directories(rts_scale_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(rts_scale_benchmark psapi)
endif()

add_executable(rts_balance_benchmark benchmark/balance_benchmark.cpp)
target_link_libraries(rts_balance_benchmark rts_simulation)
target_include_directories(rts_balance_benchmark PUBLIC ${CMAKE_SOURCE_DIR}/src)

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
    RUNTIME DESTINATION lib
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
    tests/test_fuel_supply.cpp
    tests/test_radio_events.cpp
    tests/test_match_flow.cpp
    tests/test_ai_doctrine.cpp
    tests/test_territorial_control.cpp
    tests/test_stats.cpp
    tests/test_unit_cap.cpp
    tests/test_unit_data.cpp
    tests/test_unit_separation.cpp
    tests/test_framework.hpp
)
target_link_libraries(rts_integration_tests rts_simulation)
target_include_directories(rts_integration_tests PUBLIC ${CMAKE_SOURCE_DIR}/src)
add_test(NAME rts_integration_tests COMMAND rts_integration_tests)
set_tests_properties(rts_integration_tests PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

# Equal-budget faction matchups. Failing means a unit data change broke balance.
add_test(NAME faction_balance COMMAND rts_balance_benchmark)
set_tests_properties(faction_balance PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

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
