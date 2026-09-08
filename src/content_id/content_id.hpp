#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace rts {

struct ContentHandle {
    std::string id;
    std::string content_type;
    std::string namespace_id;
    std::string identifier;
    
    std::string to_string() const;
};

class ContentRegistry {
public:
    ContentRegistry() = default;
    
    ContentHandle register_content(const std::string& type, const std::string& namespace_id, const std::string& identifier);
    bool has_collision(const std::string& id) const;
    bool has_content(const std::string& id) const;
    ContentHandle get_handle(const std::string& id) const;
    
    size_t registry_size() const { return registry_.size(); }
    const std::unordered_set<std::string>& collision_ids() const { return collision_ids_; }
    
private:
    std::unordered_map<std::string, ContentHandle> registry_;
    std::unordered_set<std::string> collision_ids_;
    
    static std::string compute_content_id(const std::string& type, const std::string& namespace_id, const std::string& identifier);
};

} // namespace rts
