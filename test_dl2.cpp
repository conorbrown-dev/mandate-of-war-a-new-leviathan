#include <dlfcn.h>
#include <iostream>

int main() {
    void* lib = dlopen("godot/project/librts_simulation.so", RTLD_NOW);
    if (!lib) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }
    
    std::cout << "dlopen success" << std::endl;
    
    void* sim_start = dlsym(lib, "simulation_start");
    void* sim_stop = dlsym(lib, "simulation_stop");
    void* sim_update = dlsym(lib, "simulation_update");
    void* sim_create_unit = dlsym(lib, "simulation_create_unit");
    void* sim_move_unit = dlsym(lib, "simulation_move_unit");
    void* sim_destroy_unit = dlsym(lib, "simulation_destroy_unit");
    void* sim_entity_count = dlsym(lib, "simulation_entity_count");
    void* sim_get_unit_x = dlsym(lib, "simulation_get_unit_x");
    void* sim_get_unit_y = dlsym(lib, "simulation_get_unit_y");
    void* logistics_add_airbase = dlsym(lib, "simulation_logistics_add_airbase");
    void* logistics_add_carrier = dlsym(lib, "simulation_logistics_add_carrier");
    void* logistics_update_intelligence = dlsym(lib, "simulation_logistics_update_intelligence");
    
    if (!sim_start || !sim_stop || !sim_update || !sim_create_unit ||
        !sim_move_unit || !sim_destroy_unit || !sim_entity_count ||
        !sim_get_unit_x || !sim_get_unit_y || !logistics_add_airbase ||
        !logistics_add_carrier || !logistics_update_intelligence) {
        
        std::cerr << "Some functions not found:" << std::endl;
        std::cerr << "  simulation_start: " << (sim_start ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_stop: " << (sim_stop ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_update: " << (sim_update ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_create_unit: " << (sim_create_unit ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_move_unit: " << (sim_move_unit ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_destroy_unit: " << (sim_destroy_unit ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_entity_count: " << (sim_entity_count ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_get_unit_x: " << (sim_get_unit_x ? "OK" : "MISSING") << std::endl;
        std::cerr << "  simulation_get_unit_y: " << (sim_get_unit_y ? "OK" : "MISSING") << std::endl;
        std::cerr << "  logistics_add_airbase: " << (logistics_add_airbase ? "OK" : "MISSING") << std::endl;
        std::cerr << "  logistics_add_carrier: " << (logistics_add_carrier ? "OK" : "MISSING") << std::endl;
        std::cerr << "  logistics_update_intelligence: " << (logistics_update_intelligence ? "OK" : "MISSING") << std::endl;
        std::cerr << "dlerror: " << (dlerror() ? dlerror() : "NULL") << std::endl;
        dlclose(lib);
        return 1;
    }
    
    std::cout << "All functions loaded successfully" << std::endl;
    dlclose(lib);
    return 0;
}
