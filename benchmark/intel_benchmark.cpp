#include "simulation/simulation.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace rts;
int main(int argc, char** argv) {
    try {
        const bool intel = argc < 2 || std::stoi(argv[1]) != 0;
        Simulation sim; sim.configure_world_size(40000,40000); sim.start(); sim.enable_ai(false);
        const auto a=sim.create_faction_base(FactionId::ELITE_PRECISION,-10000,-10000);
        const auto b=sim.create_faction_base(FactionId::MASS_WARFARE,10000,10000);
        std::vector<EntityId> ids; std::vector<float> before;
        for(int side=0;side<2;++side) for(int i=0;i<500;++i) {
            const float x=(side ? 1 : -1)*(130.0f+float(i%10)*12.0f);
            const float y=float(i/10)*15.0f;
            const auto id=sim.create_unit_with_type(x,y,side?UnitType::MASS_SWARM_TANK:UnitType::ELITE_MAIN_BATTLE_TANK,static_cast<FactionId>(side));
            if(id<0) throw std::runtime_error("spawn rejected");
            if(!intel) sim.component_manager().get_component<UnitData>(id)->intel_range=0;
            // Keep both sides active without closing to weapon range.
            sim.move_unit(id,x,y+500);
            ids.push_back(id); before.push_back(y);
        }
        sim.begin_match(a,b);
        std::vector<double> samples;
        bool contact=false;
        for(int i=0;i<100;++i) {
            const auto start=std::chrono::steady_clock::now(); sim.update(50);
            samples.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            contact=contact||!sim.radar_contacts(FactionId::ELITE_PRECISION).empty();
        }
        int moved=0;for(size_t i=0;i<ids.size();++i) if(sim.get_unit_y(ids[i])-before[i]>1) ++moved;
        if(moved<900 || contact!=intel) throw std::runtime_error("active movement/contact assertions failed");
        double total=0;for(double sample:samples) total+=sample;
        std::sort(samples.begin(),samples.end());
        std::cout<<"INTEL_BENCHMARK enabled="<<intel<<" live_moving_units="<<ids.size()<<" ticks=100 moved="<<moved
                 <<" contacts_observed="<<contact<<" average_ms="<<total/samples.size()<<" p95_ms="<<samples[94]<<" max_ms="<<samples.back()<<" hash="<<sim.state_hash()<<"\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}
}
