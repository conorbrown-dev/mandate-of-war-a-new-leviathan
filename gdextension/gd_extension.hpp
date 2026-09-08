// Minimal RtsExtension GDExtension implementation
// Uses C interface directly - no godot-cpp dependency

#include <stdbool.h>
#include <stdint.h>

// Export macro for shared library
#if defined(_WIN32)
    #define GDE_EXPORT __declspec(dllexport)
#else
    #define GDE_EXPORT __attribute__((visibility("default")))
#endif

// GDExtension types
typedef int GDExtensionBool;
typedef void* GDExtensionClassLibraryPtr;
typedef void* GDExtensionInitializationPtr;

// Forward declare C++ simulation types
struct EntityId {
    int value;
};

struct Vector3 {
    float x, y, z;
};

// External C API from simulation library (C-compatible wrappers)
extern "C" {
    // Simulation lifecycle - C-compatible wrappers around rts::Simulation
    void simulation_start();
    void simulation_stop();
    void simulation_update(float delta_ms);
    
    // Entity management - C-compatible wrappers
    int simulation_create_unit(float x, float y);
    void simulation_move_unit(int entity_id, float x, float y);
    int simulation_entity_count();
    float simulation_get_unit_x(int entity_id);
    float simulation_get_unit_y(int entity_id);
}

// Wrapper class for Godot binding
class RtsExtension {
public:
    static void init();
    static void deinit();
    static void update(float delta_ms);
    static int create_unit(float x, float y);
    static void move_unit(int entity_id, float x, float y);
    static int get_entity_count();
    static float get_unit_x(int entity_id);
    static float get_unit_y(int entity_id);
};

// GDExtension library entry point
extern "C" {

GDE_EXPORT GDExtensionBool example_library_init(
    const void *p_interface,
    GDExtensionClassLibraryPtr p_library,
    GDExtensionInitializationPtr r_init
) {
    RtsExtension::init();
    return 0;  // GDExtensionBool(false)
}

GDE_EXPORT void example_library_deinit(GDExtensionClassLibraryPtr p_library) {
    RtsExtension::deinit();
}

} // extern "C"
