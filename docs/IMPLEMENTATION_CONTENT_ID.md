# Stable Content ID Registry

## Objective

Implement deterministic content ID generation with SHA-256 and a registry system for modding.

## Implementation Plan

### 1. Content ID Helper (`src/content_id.hpp`)

```cpp
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

struct ContentHandle {
    std::string id;
    std::string content_type;
    std::string namespace_id;
    std::string identifier;
    
    std::string to_string() const;
};

class ContentRegistry {
public:
    ContentHandle register_content(std::string_view type, std::string_view namespace_id, std::string_view identifier);
    bool has_collision(std::string_view id) const;
    bool has_content(std::string_view id) const;
    ContentHandle get_handle(std::string_view id) const;
    std::string generate_content_id(std::string_view type, std::string_view namespace_id, std::string_view identifier) const;
    
private:
    std::unordered_map<std::string, ContentHandle> registry_;
    std::unordered_set<std::string> collision_ids_;
    
    static std::string compute_content_id(std::string_view type, std::string_view namespace_id, std::string_view identifier);
};

```

### 2. Test Harness (`benchmark/content_id_benchmark.cpp`)

```cpp
#include "content_id.hpp"
#include <benchmark/benchmark.h>
#include <random>

static void BM_ContentIDGeneration(benchmark::State& state) {
    ContentRegistry registry;
    std::string type = "unit";
    std::string ns = "faction_a";
    std::string id = "t1_interceptor";
    
    for (auto _ : state) {
        auto handle = registry.register_content(type, ns, id);
        benchmark::DoNotOptimize(handle.id);
    }
}

static void BM_ContentIDCollisionCheck(benchmark::State& state) {
    ContentRegistry registry;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 100000);
    
    std::vector<std::string> ids;
    for (int i = 0; i < 1000; ++i) {
        ids.push_back(registry.register_content("unit", "faction_" + std::to_string(i % 10), "unit_" + std::to_string(i)));
    }
    
    for (auto _ : state) {
        auto id = ids[dis(gen)];
        benchmark::DoNotOptimize(registry.has_content(id));
    }
}

BENCHMARK(BM_ContentIDGeneration);
BENCHMARK(BM_ContentIDCollisionCheck);

BENCHMARK_MAIN();
```

### 3. Implementation (`src/content_id.cpp`)

```cpp
#include "content_id.hpp"
#include <crypto++/sha.h>
#include <crypto++/hex.h>
#include <sstream>
#include <iomanip>

std::string ContentHandle::to_string() const {
    return id + "|" + content_type + "|" + namespace_id + "|" + identifier;
}

std::string ContentRegistry::compute_content_id(std::string_view type, std::string_view namespace_id, std::string_view identifier) {
    std::string input = std::string(type) + "|" + std::string(namespace_id) + "|" + std::string(identifier);
    
    CryptoPP::SHA256 hash;
    std::string digest(hash.DigestSize(), 0);
    hash.CalculateDigest(reinterpret_cast<byte*>(&digest[0]), reinterpret_cast<const byte*>(input.data()), input.size());
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 8; ++i) {
        ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(digest[i]));
    }
    return ss.str();
}

ContentHandle ContentRegistry::register_content(std::string_view type, std::string_view namespace_id, std::string_view identifier) {
    std::string id = compute_content_id(type, namespace_id, identifier);
    
    if (registry_.find(id) != registry_.end()) {
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

bool ContentRegistry::has_collision(std::string_view id) const {
    return collision_ids_.find(id) != collision_ids_.end();
}

bool ContentRegistry::has_content(std::string_view id) const {
    return registry_.find(id) != registry_.end();
}

ContentHandle ContentRegistry::get_handle(std::string_view id) const {
    auto it = registry_.find(id);
    if (it != registry_.end()) {
        return it->second;
    }
    return ContentHandle{};
}
```

### 4. Tests (`tests/content_id_tests.cpp`)

```cpp
#include "content_id.hpp"
#include <gtest/gtest.h>
#include <set>

TEST(ContentIDTests, DeterministicIDGeneration) {
    ContentRegistry registry;
    
    auto h1 = registry.register_content("unit", "faction_a", "t1_interceptor");
    auto h2 = registry.register_content("unit", "faction_a", "t1_interceptor");
    
    EXPECT_EQ(h1.id, h2.id);
    EXPECT_EQ(h1.id, "b7a38f2e");
}

TEST(ContentIDTests, UniqueIDsForDifferentContent) {
    ContentRegistry registry;
    
    auto h1 = registry.register_content("unit", "faction_a", "t1_interceptor");
    auto h2 = registry.register_content("unit", "faction_a", "t1_interceptor_different");
    auto h3 = registry.register_content("unit", "faction_b", "t1_interceptor");
    
    EXPECT_NE(h1.id, h2.id);
    EXPECT_NE(h1.id, h3.id);
    EXPECT_NE(h2.id, h3.id);
}

TEST(ContentIDTests, CollisionDetection) {
    ContentRegistry registry;
    
    registry.register_content("unit", "faction_a", "t1_interceptor");
    
    auto h2 = registry.register_content("weapon", "faction_a", "t1_interceptor");
    
    EXPECT_TRUE(registry.has_collision(h2.id));
}

TEST(ContentIDTests, QueryById) {
    ContentRegistry registry;
    
    auto handle = registry.register_content("unit", "faction_a", "t1_interceptor");
    
    EXPECT_TRUE(registry.has_content(handle.id));
    auto retrieved = registry.get_handle(handle.id);
    EXPECT_EQ(retrieved.id, handle.id);
    EXPECT_EQ(retrieved.content_type, "unit");
}

TEST(ContentIDTests, ManyIDsNoCollisions) {
    ContentRegistry registry;
    std::set<std::string> ids;
    
    for (int i = 0; i < 10000; ++i) {
        std::string type = (i % 2 == 0) ? "unit" : "faction";
        std::string ns = "faction_" + std::to_string(i % 100);
        std::string id = "unit_" + std::to_string(i);
        
        auto handle = registry.register_content(type, ns, id);
        ids.insert(handle.id);
    }
    
    EXPECT_EQ(ids.size(), 10000);
    EXPECT_EQ(registry.collision_ids_.size(), 0);
}

```

### 5. CMakeLists.txt Update

```cmake
add_executable(rts_content_id_benchmark benchmark/content_id_benchmark.cpp src/content_id.cpp)
target_link_libraries(rts_content_id_benchmark benchmark::benchmark CryptoPP)
add_test(rts_content_id_benchmark rts_content_id_benchmark)

add_executable(rts_content_id_tests tests/content_id_tests.cpp src/content_id.cpp)
target_link_libraries(rts_content_id_tests GTest::gtest_main CryptoPP)
add_test(rts_content_id_tests rts_content_id_tests)
```

## Verification

Run:
```bash
./build/rts_content_id_tests
./build/rts_content_id_benchmark
```

Expected: 100% tests pass, benchmark shows < 1μs per ID generation.
