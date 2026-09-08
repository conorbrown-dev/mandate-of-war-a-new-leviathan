#include "json_parser.hpp"
#include <cctype>
#include <stdexcept>

namespace rts::data {

JsonValue::JsonValue() : type_(Type::Null), value_(std::monostate{}) {}
JsonValue::JsonValue(bool b) : type_(Type::Bool), value_(b) {}
JsonValue::JsonValue(double n) : type_(Type::Number), value_(n) {}
JsonValue::JsonValue(const std::string& s) : type_(Type::String), value_(s) {}
JsonValue::JsonValue(const char* s) : type_(Type::String), value_(std::string(s)) {}
JsonValue::JsonValue(std::vector<JsonValue> arr) : type_(Type::Array), value_(std::move(arr)) {}
JsonValue::JsonValue(std::unordered_map<std::string, JsonValue> obj) 
    : type_(Type::Object), value_(std::move(obj)) {}

JsonValue::Type JsonValue::type() const { return type_; }

bool JsonValue::as_bool() const {
    if (type_ != Type::Bool) throw std::runtime_error("Not a bool");
    return std::get<bool>(value_);
}

double JsonValue::as_number() const {
    if (type_ != Type::Number) throw std::runtime_error("Not a number");
    return std::get<double>(value_);
}

std::string JsonValue::as_string() const {
    if (type_ != Type::String) throw std::runtime_error("Not a string");
    return std::get<std::string>(value_);
}

const std::vector<JsonValue>& JsonValue::as_array() const {
    if (type_ != Type::Array) throw std::runtime_error("Not an array");
    return std::get<std::vector<JsonValue>>(value_);
}

const std::unordered_map<std::string, JsonValue>& JsonValue::as_object() const {
    if (type_ != Type::Object) throw std::runtime_error("Not an object");
    return std::get<std::unordered_map<std::string, JsonValue>>(value_);
}

std::optional<JsonValue> JsonValue::get(const std::string& key) const {
    if (type_ != Type::Object) return std::nullopt;
    auto it = std::get<std::unordered_map<std::string, JsonValue>>(value_).find(key);
    if (it == std::get<std::unordered_map<std::string, JsonValue>>(value_).end()) return std::nullopt;
    return it->second;
}

size_t JsonValue::array_size() const {
    if (type_ != Type::Array) throw std::runtime_error("Not an array");
    return std::get<std::vector<JsonValue>>(value_).size();
}

static std::string trim(const std::string& s) {
    size_t start = 0, end = s.size();
    while (start < end && std::isspace(s[start])) start++;
    while (end > start && std::isspace(s[end-1])) end--;
    return s.substr(start, end - start);
}

static bool skip_whitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(json[pos])) pos++;
    return pos < json.size();
}

static std::optional<char> peek(const std::string& json, size_t pos) {
    if (pos < json.size()) return json[pos];
    return std::nullopt;
}

static char consume(const std::string& json, size_t& pos) {
    return json[pos++];
}

static bool match(const std::string& json, size_t& pos, char c) {
    if (pos >= json.size() || json[pos] != c) return false;
    pos++;
    return true;
}

static std::optional<std::string> parse_string(const std::string& json, size_t& pos) {
    if (!match(json, pos, '"')) return std::nullopt;
    std::string result;
    while (pos < json.size()) {
        char c = consume(json, pos);
        if (c == '"') return result;
        if (c == '\\' && pos < json.size()) {
            char escaped = consume(json, pos);
            switch (escaped) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case '/': result += '/'; break;
                case 'b': result += '\b'; break;
                case 'f': result += '\f'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: return std::nullopt;
            }
        } else {
            result += c;
        }
    }
    return std::nullopt;
}

static std::optional<double> parse_number(const std::string& json, size_t& pos) {
    size_t start = pos;
    if (pos < json.size() && json[pos] == '-') pos++;
    while (pos < json.size() && std::isdigit(json[pos])) pos++;
    if (pos < json.size() && json[pos] == '.') {
        pos++;
        while (pos < json.size() && std::isdigit(json[pos])) pos++;
    }
    if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
        pos++;
        if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
        while (pos < json.size() && std::isdigit(json[pos])) pos++;
    }
    try {
        return std::stod(json.substr(start, pos - start));
    } catch (...) {
        return std::nullopt;
    }
}

static std::optional<JsonValue> parse_value(const std::string& json, size_t& pos);

static std::optional<JsonValue> parse_object(const std::string& json, size_t& pos) {
    if (!match(json, pos, '{')) return std::nullopt;
    std::unordered_map<std::string, JsonValue> obj;
    
    skip_whitespace(json, pos);
    if (pos < json.size() && json[pos] == '}') {
        pos++;
        return JsonValue(std::move(obj));
    }
    
    while (true) {
        skip_whitespace(json, pos);
        auto key_opt = parse_string(json, pos);
        if (!key_opt) return std::nullopt;
        
        if (!match(json, pos, ':')) return std::nullopt;
        
        auto value_opt = parse_value(json, pos);
        if (!value_opt) return std::nullopt;
        
        obj[key_opt.value()] = value_opt.value();
        
        skip_whitespace(json, pos);
        if (match(json, pos, '}')) return JsonValue(std::move(obj));
        if (!match(json, pos, ',')) return std::nullopt;
    }
}

static std::optional<JsonValue> parse_array(const std::string& json, size_t& pos) {
    if (!match(json, pos, '[')) return std::nullopt;
    std::vector<JsonValue> arr;
    
    skip_whitespace(json, pos);
    if (pos < json.size() && json[pos] == ']') {
        pos++;
        return JsonValue(std::move(arr));
    }
    
    while (true) {
        skip_whitespace(json, pos);
        auto value_opt = parse_value(json, pos);
        if (!value_opt) return std::nullopt;
        arr.push_back(value_opt.value());
        
        skip_whitespace(json, pos);
        if (match(json, pos, ']')) return JsonValue(std::move(arr));
        if (!match(json, pos, ',')) return std::nullopt;
    }
}

static std::optional<JsonValue> parse_value(const std::string& json, size_t& pos) {
    skip_whitespace(json, pos);
    
    char c = json[pos];
    if (c == '"') {
        auto str = parse_string(json, pos);
        if (!str) return std::nullopt;
        return JsonValue(str.value());
    }
    if (c == '{') return parse_object(json, pos);
    if (c == '[') return parse_array(json, pos);
    if (c == 't') {
        if (json.substr(pos, 4) == "true") { pos += 4; return JsonValue(true); }
        return std::nullopt;
    }
    if (c == 'f') {
        if (json.substr(pos, 5) == "false") { pos += 5; return JsonValue(false); }
        return std::nullopt;
    }
    if (c == 'n') {
        if (json.substr(pos, 4) == "null") { pos += 4; return JsonValue(); }
        return std::nullopt;
    }
    if (std::isdigit(c) || c == '-') {
        auto num = parse_number(json, pos);
        if (!num) return std::nullopt;
        return JsonValue(num.value());
    }
    return std::nullopt;
}

std::optional<JsonValue> JsonParser::parse(const std::string& json) {
    size_t pos = 0;
    skip_whitespace(json, pos);
    auto result = parse_value(json, pos);
    if (!result) return std::nullopt;
    skip_whitespace(json, pos);
    if (pos == json.size()) return result;
    return std::nullopt;
}

} // namespace rts::data
