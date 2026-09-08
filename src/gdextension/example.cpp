#include <godot/godot.hpp>
#include <godot/cpp/core/class_db.hpp>

#include "simulation/simulation.hpp"
#include "gdextension/gd_extension.hpp"

namespace godot {

void GDExtensionExample::_bind_methods() {
    ClassDB::bind_method(D_METHOD("start"), &GDExtensionExample::start);
    ClassDB::bind_method(D_METHOD("stop"), &GDExtensionExample::stop);
    ClassDB::bind_method(D_METHOD("create_unit", "x", "y"), &GDExtensionExample::create_unit);
}

GDExtensionExample::GDExtensionExample() {}

GDExtensionExample::~GDExtensionExample() {}

void GDExtensionExample::start() {
    simulation_.start();
}

void GDExtensionExample::stop() {
    simulation_.stop();
}

int GDExtensionExample::create_unit(double x, double y) {
    Entity entity = simulation_.create_unit(x, y);
    return static_cast<int>(entity.id);
}

} // namespace godot

extern "C" {
GDExtensionBool GDE_EXPORT example_library_init(const GDExtensionInterface *p_interface, GDExtensionClassLibraryPtr p_library, GDExtensionInitialization *r_init) {
    godot::Godot::init(p_interface, p_library, r_init);
    
    r_init->register_class<godot::GDExtensionExample>();
    
    return GDExtensionBool(false);
}

void GDE_EXPORT example_library_deinit(GDExtensionClassLibraryPtr p_library) {
    godot::Godot::deinit();
}
}