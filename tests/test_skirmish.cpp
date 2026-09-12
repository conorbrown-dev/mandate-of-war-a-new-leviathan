#include "test_framework.hpp"
#include "simulation/skirmish.hpp"
#include <fstream>
#include <filesystem>
using namespace rts;
namespace {
int checks=0;
void verify(bool condition,const char* reason) { ++checks; if(!condition) throw std::runtime_error(reason); }
}
TEST(skirmish_validated_setup_and_repeatable_legal_terminal) {
    Simulation a,b; Skirmish first(a),second(b);
    namespace fs = std::filesystem;
    fs::path scenario_path = fs::current_path() / "godot" / "project" / "scenarios" / "two_landmass_skirmish.json";
    std::string path = fs::absolute(scenario_path).string();
    first.load(path); second.load(path);
    verify(a.entity_count()==51 && b.entity_count()==51, "scenario roster plus two owned bases instantiated");
    verify(!first.load("/tmp/missing-g08-scenario.json") && a.entity_count()==51, "failed load preserves match");
    int tick=0;
    while(first.result()==-1 && tick++<20000) {
        first.update(50); second.update(50);
        const auto sa=a.get_state(),sb=b.get_state();
        verify(sa.entity_ids==sb.entity_ids && sa.positions_x==sb.positions_x && sa.positions_y==sb.positions_y && sa.health_current==sb.health_current,
               "identical matches agree on entity state each tick");
    }
    if(first.result()==-1) {
        std::cout << "G08 stalled entities=" << a.entity_count() << " builds=" << a.production_manager().get_construction_count() << '\n';
        int shown=0;
        for(auto id:a.get_entity_list()) if(shown++<70) std::cout << id << ": " << a.get_unit_x(id) << "," << a.get_unit_y(id) << '\n';
    }
    verify(first.result()!=-1, "legal AI and combat reach terminal state within 1000 simulation seconds");
    verify(first.result()==second.result(), "identical matches agree on result");
    verify(a.production_manager().get_construction_count()>0, "opponent produces units during match");
    verify(!a.production_manager().research(FactionId::MASS_WARFARE).completed_projects.empty(), "opponent completes research during match");
    verify(first.checksums()==second.checksums(), "full recorded checksum sequences agree");
    const auto replay_path=(std::filesystem::temp_directory_path()/"g08-match-test.replay").string();
    verify(first.save_replay(replay_path), "completed match replay saved");
    verify(first.replay(replay_path), first.error().c_str());
    {
        std::ofstream corrupt(replay_path,std::ios::app); corrupt << 'x';
    }
    verify(!first.replay(replay_path), "corrupt replay rejected before live mutation");
    verify(first.result()==second.result() && a.simulation_tick()==b.simulation_tick(), "corrupt replay preserves completed match");
    std::filesystem::remove(replay_path);
    auto final_tick=a.simulation_tick();first.update(250);
    verify(a.simulation_tick()==final_tick,"terminal match freezes simulation");
    std::cout << "G08 match behavior checks=" << checks << " terminal_tick=" << final_tick << " winner=" << first.result() << '\n';
}
TEST(skirmish_logistics_orders_recover_and_resupply) {
    Simulation simulation; Skirmish match(simulation);
    verify(match.load("godot/project/scenarios/two_landmass_skirmish.json"), "logistics scenario loads");
    EntityId aircraft=INVALID_ENTITY,vessel=INVALID_ENTITY,ground=INVALID_ENTITY;
    for(auto id:simulation.get_entity_list()) {
        if(simulation.component_manager().get_component<Aircraft>(id)) aircraft=id;
        if(simulation.component_manager().get_component<NavalVessel>(id)) vessel=id;
        if(id!=match.base(0) && simulation.component_manager().get_component<UnitData>(id) &&
           !simulation.component_manager().get_component<Aircraft>(id) && !simulation.component_manager().get_component<NavalVessel>(id)) ground=id;
    }
    verify(aircraft!=INVALID_ENTITY && vessel!=INVALID_ENTITY,"scenario contains aircraft and vessel");
    verify(simulation.issue_commands({aircraft},FactionId::ELITE_PRECISION,CommandType::MOVE,-80,20)==1,"move launches supported aircraft");
    for(int i=0;i<60;++i) match.update(50);
    auto* plane=simulation.component_manager().get_component<Aircraft>(aircraft);
    verify(plane && plane->status==Aircraft::Status::AIRBORNE,"aircraft completes runway takeoff");
    verify(simulation.issue_return_commands({aircraft},FactionId::ELITE_PRECISION)==1,"authoritative recovery accepted");
    match.update(50);
    for(int i=0;i<159;++i) match.update(50);
    plane=simulation.component_manager().get_component<Aircraft>(aircraft);
    if (plane) std::cout << "recovery status=" << int(plane->status) << " fuel=" << plane->fuel << " max=" << plane->max_fuel << " x=" << plane->x << " y=" << plane->y << "\n";
    verify(plane && plane->status==Aircraft::Status::ON_GROUND && plane->fuel==plane->max_fuel,"recovery lands and refuels aircraft");
    for(int i=0;i<201;++i) match.update(50);
    auto* boat=simulation.component_manager().get_component<NavalVessel>(vessel);
    verify(boat && boat->is_stranded,"scenario vessel exhausts endurance and strands");
    auto old_funds=simulation.production_manager().storages().at(match.base(0)).energy_storage;
    verify(simulation.issue_return_commands({vessel},FactionId::ELITE_PRECISION)==1,"in-range stranded vessel can request paid resupply");
    match.update(50);
    verify(!boat->is_stranded && boat->fuel>90,"paid resupply restores operational vessel");
    verify(simulation.production_manager().storages().at(match.base(0)).energy_storage<old_funds,"resupply consumes owned energy");
    verify(simulation.issue_commands({vessel},FactionId::ELITE_PRECISION,CommandType::MOVE,-105,0)==0,"naval orders cannot enter land");
    verify(!simulation.pathfinding().is_walkable(simulation.pathfinding().to_grid_x(0),simulation.pathfinding().to_grid_y(40)),"sea blocks ground routing");
    verify(simulation.pathfinding().is_walkable(simulation.pathfinding().to_grid_x(0),simulation.pathfinding().to_grid_y(0)),"causeway connects landmasses");
}

TEST(skirmish_time_limit_draw_replays) {
    Simulation simulation; Skirmish match(simulation);
    verify(match.load("godot/project/scenarios/two_landmass_skirmish.json"),"draw scenario starts");
    simulation.enable_ai(false);
    while(match.result()<0) match.update(250);
    verify(simulation.simulation_tick()==Skirmish::MAX_MATCH_TICKS && match.result()==2,"surviving command centers draw at documented limit");
    const auto path=(std::filesystem::temp_directory_path()/"g08-draw.replay").string();
    verify(match.save_replay(path),"longest legal match saves");
    verify(match.replay(path),match.error().c_str());
    std::filesystem::remove(path);
}
