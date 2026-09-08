#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>

namespace rts::data {

class JsonValue;

class JsonParser {
public:
    static std::optional<JsonValue> parse(const std::string& json);
};

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };
    
    JsonValue();
    explicit JsonValue(bool b);
    explicit JsonValue(double n);
    explicit JsonValue(const std::string& s);
    explicit JsonValue(const char* s);
    explicit JsonValue(std::vector<JsonValue> arr);
    explicit JsonValue(std::unordered_map<std::string, JsonValue> obj);
    
    Type type() const;
    
    bool as_bool() const;
    double as_number() const;
    std::string as_string() const;
    const std::vector<JsonValue>& as_array() const;
    const std::unordered_map<std::string, JsonValue>& as_object() const;
    
    std::optional<JsonValue> get(const std::string& key) const;
    size_t array_size() const;
    
private:
    Type type_;
    std::variant<std::monostate, bool, double, std::string, std::vector<JsonValue>, std::unordered_map<std::string, JsonValue>> value_;
};

} // namespace rts::data
