#include "test_framework.hpp"
#include "content_id/content_id.hpp"
#include "util/sha256.hpp"
#include <iomanip>
#include <sstream>
#include <string>

static std::string sha256_hex(const std::string& input) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), digest);
    std::ostringstream out;
    for (unsigned char byte : digest) {
        out << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return out.str();
}

// Published FIPS 180-4 vectors, plus lengths either side of the padding split.
TEST(sha256_matches_published_vectors) {
    const std::pair<std::string, std::string> vectors[] = {
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        {std::string(1000000, 'a'),
         "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"},
    };
    for (const auto& [input, expected] : vectors) {
        if (sha256_hex(input) != expected) {
            throw std::runtime_error("SHA-256 digest mismatch for input of size " + std::to_string(input.size()));
        }
    }
}

TEST(content_id_deterministic_generation) {
    rts::ContentRegistry registry;
    
    auto h1 = registry.register_content("unit", "faction_a", "t1_interceptor");
    auto h2 = registry.register_content("unit", "faction_a", "t1_interceptor");
    
    if (h1.id != h2.id) {
        throw std::runtime_error("Same content must produce same ID");
    }
    if (registry.has_collision(h1.id) || registry.registry_size() != 1) {
        throw std::runtime_error("Idempotent registration must not create a collision or duplicate entry");
    }
}

TEST(content_id_unique_for_different_content) {
    rts::ContentRegistry registry;
    
    auto h1 = registry.register_content("unit", "faction_a", "t1_interceptor");
    auto h2 = registry.register_content("unit", "faction_a", "t1_interceptor_alt");
    auto h3 = registry.register_content("unit", "faction_b", "t1_interceptor");
    
    if (h1.id == h2.id || h1.id == h3.id || h2.id == h3.id) {
        throw std::runtime_error("Different content must produce different IDs");
    }
}

TEST(content_id_collision_detection) {
    rts::ContentRegistry registry;
    
    registry.register_content("unit", "faction_a", "t1_interceptor");
    auto h2 = registry.register_content("weapon", "faction_a", "t1_interceptor");
    
    if (h2.id != "0ffc0d8ace5b00dc2caf3fd2daf0c2c59b0eb4274f83821310a19f3e7ce95790") {
        std::string msg = "Fixed collision ID verification, got: " + h2.id;
        throw std::runtime_error(msg);
    }
}

TEST(content_id_query_by_id) {
    rts::ContentRegistry registry;
    
    auto handle = registry.register_content("unit", "faction_a", "t1_interceptor");
    
    if (!registry.has_content(handle.id)) {
        throw std::runtime_error("Query by ID must return true for registered content");
    }
    
    auto retrieved = registry.get_handle(handle.id);
    if (retrieved.id != handle.id) {
        throw std::runtime_error("Retrieved handle must match original");
    }
}

TEST(content_id_many_registrations) {
    rts::ContentRegistry registry;
    
    for (int i = 0; i < 1000; ++i) {
        std::string type = (i % 2 == 0) ? "unit" : "faction";
        std::string ns = "faction_" + std::to_string(i % 100);
        std::string id = "unit_" + std::to_string(i);
        
        registry.register_content(type, ns, id);
    }
    
    if (registry.registry_size() != 1000) {
        throw std::runtime_error("Registry must contain all 1000 entries");
    }
    
    if (registry.collision_ids().size() > 0) {
        throw std::runtime_error("No collisions expected for unique content");
    }
}
