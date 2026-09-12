#include "simulation/skirmish.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <stdexcept>
using namespace rts;
int main() {
    try {
        Simulation simulation; Skirmish match(simulation);
        if(!match.load("godot/project/scenarios/two_landmass_skirmish.json")) throw std::runtime_error(match.error());
        std::vector<EntityId> movers;
        for(int f=0;f<2;++f) for(int i=0;i<500;++i) {
            const float x=(f==0?-1:1)*(12.0f+float(i%50)*0.5f);
            const float y=-6.0f+float(i/50)*1.2f;
            const auto id=simulation.create_unit_with_type(x,y,f==0?UnitType::ELITE_MAIN_BATTLE_TANK:UnitType::MASS_SWARM_TANK,static_cast<FactionId>(f));
            simulation.stop_unit(id); movers.push_back(id);
        }
        const auto initial=simulation.get_state();
        std::vector<double> samples;
        size_t moving_samples=0, projectile_samples=0;
        for(int tick=0;tick<400;++tick) {
            // Stagger real authoritative ingress within the fixed 256-command budget.
            if(tick<10) for(int index=tick*100;index<(tick+1)*100;++index) {
                const int faction=index/500;
                if(simulation.issue_commands({movers[index]},static_cast<FactionId>(faction),CommandType::MOVE,
                    faction==0?30.0f:-30.0f,0)!=1) throw std::runtime_error("Benchmark movement command rejected");
            }
            const auto start=std::chrono::steady_clock::now();
            match.update(50);
            const double elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            samples.push_back(elapsed);
            const auto state=simulation.get_state();
            if(std::any_of(state.velocities_x.begin(),state.velocities_x.end(),[](float v){return v!=0;})) ++moving_samples;
            if(simulation.combat_manager().projectile_manager().active_count()>0) ++projectile_samples;
            if(match.result()!=-1) throw std::runtime_error("Workload ended before measurement completed");
        }
        const auto final=simulation.get_state();
        if(initial.entity_ids.size()<1000 || moving_samples<100 || projectile_samples==0 ||
            simulation.production_manager().get_construction_count()==0 || final.entity_ids.size()>=initial.entity_ids.size())
            throw std::runtime_error("Workload lacks required movement/combat/destruction/production");
        std::sort(samples.begin(),samples.end());
        const double average=std::accumulate(samples.begin(),samples.end(),0.0)/samples.size();
        std::cout << "G08 active skirmish: initial=" << initial.entity_ids.size() << " survivors=" << final.entity_ids.size()
                  << " ticks=" << samples.size() << " moving_ticks=" << moving_samples << " projectile_ticks=" << projectile_samples
                  << " builds=" << simulation.production_manager().get_construction_count() << '\n';
        std::cout << "ms/tick avg=" << average << " p50=" << samples[samples.size()/2]
                  << " p95=" << samples[samples.size()*95/100] << " max=" << samples.back() << '\n';
        if(samples.back()>50) throw std::runtime_error("Maximum tick exceeds 50 ms budget");
        return 0;
    } catch(const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
