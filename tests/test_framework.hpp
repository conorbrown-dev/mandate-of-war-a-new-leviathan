#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <stdexcept>

namespace rts {
namespace test {

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

class TestRunner {
public:
    void add_test(const std::string& name, std::function<void()> test_fn) {
        tests_.push_back({name, false, ""});
        test_functions_[name] = test_fn;
    }
    
    void run_all() {
        int passed = 0;
        int failed = 0;
        std::cout << "Running tests...\n";
        
        for (auto& test : tests_) {
            try {
                test_functions_[test.name]();
                test.passed = true;
                test.message = "OK";
                passed++;
                std::cout << "[PASS] " << test.name << "\n";
            } catch (const std::exception& e) {
                test.passed = false;
                test.message = e.what();
                failed++;
                std::cout << "[FAIL] " << test.name << ": " << test.message << "\n";
            }
        }
        
        std::cout << "\nResults: " << passed << " passed, " << failed << " failed\n";
        
        if (failed > 0) {
            throw std::runtime_error("Tests failed");
        }
    }

private:
    std::vector<TestResult> tests_;
    std::unordered_map<std::string, std::function<void()>> test_functions_;
};

#define TEST(name) \
    void test_##name(); \
    namespace { \
        struct reg_##name { \
            reg_##name() { \
                rts::test::test_runner.add_test(#name, test_##name); \
            } \
        } reg_##name##_instance; \
    } \
    void test_##name()

inline TestRunner test_runner;

} // namespace test
} // namespace rts
