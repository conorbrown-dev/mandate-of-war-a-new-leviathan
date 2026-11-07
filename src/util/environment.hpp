#pragma once

#include <string>

namespace rts {

// Reads and writes the process environment itself. On Windows std::getenv sees
// a private copy made when the library loaded, which misses values set later
// by Godot's OS.set_environment. Returns an empty string when the name is unset.
std::string read_environment(const char* name);
void write_environment(const char* name, const std::string& value);

} // namespace rts
