#include "content_id.hpp"
#include <sstream>
#include <iomanip>
#include <openssl/sha.h>

namespace rts {

std::string ContentHandle::to_string() const {
    return id + "|" + content_type + "|" + namespace_id + "|" + identifier;
}

std::string ContentRegistry::compute_content_id(const std::string& type, const std::string& namespace_id, const std::string& identifier) {
    std::string input = type + "|" + namespace_id + "|" + identifier;
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);
    
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        oss << std::setw(2) << static_cast<unsigned int>(hash[i]);
    }
    
    return oss.str();
}

ContentHandle ContentRegistry::register_content(const std::string& type, const std::string& namespace_id, const std::string& identifier) {
    std::string id = compute_content_id(type, namespace_id, identifier);
    
    auto it = registry_.find(id);
    if (it != registry_.end()) {
        collision_ids_.insert(id);
    }
    
    ContentHandle handle;
    handle.id = id;
    handle.content_type = type;
    handle.namespace_id = namespace_id;
    handle.identifier = identifier;
    
    registry_[id] = handle;
    return handle;
}

bool ContentRegistry::has_collision(const std::string& id) const {
    return collision_ids_.find(id) != collision_ids_.end();
}

bool ContentRegistry::has_content(const std::string& id) const {
    return registry_.find(id) != registry_.end();
}

ContentHandle ContentRegistry::get_handle(const std::string& id) const {
    auto it = registry_.find(id);
    if (it != registry_.end()) {
        return it->second;
    }
    return ContentHandle{};
}

} // namespace rts
