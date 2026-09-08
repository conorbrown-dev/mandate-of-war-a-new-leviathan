#include <dlfcn.h>
#include <iostream>

int main() {
    void* lib = dlopen("godot/project/librts_simulation.so", RTLD_NOW);
    if (!lib) {
        std::cerr << "dlopen failed: " << dlerror() << std::endl;
        return 1;
    }
    
    void* sym = dlsym(lib, "simulation_logistics_update_intelligence");
    if (!sym) {
        std::cerr << "dlsym failed: " << dlerror() << std::endl;
        dlclose(lib);
        return 1;
    }
    
    std::cout << "Symbol found at " << sym << std::endl;
    dlclose(lib);
    return 0;
}
