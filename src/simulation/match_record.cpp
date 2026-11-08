#include "simulation/match_record.hpp"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <sstream>

#include "data/json_parser.hpp"

namespace rts {

namespace {

void write_string(std::ostringstream& out, const std::string& text) {
    out << '"';
    for (const char c : text) {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned char>(c));
                    out << escaped;
                } else {
                    out << c;
                }
        }
    }
    out << '"';
}

void write_number(std::ostringstream& out, double value) {
    if (!std::isfinite(value)) {
        out << "null";
        return;
    }
    char text[32];
    std::snprintf(text, sizeof(text), "%.9g", value);
    out << text;
}

} // namespace

void MatchRecord::clear() { *this = MatchRecord{}; }

void MatchRecord::begin(uint32_t tick, float command_position_scale) {
    clear();
    started_ = recording_ = true;
    start_tick_ = end_tick_ = tick;
    command_position_scale_ = command_position_scale;
}

void MatchRecord::add_action(uint32_t tick, std::string name, std::vector<double> args) {
    if (recording_) actions_.push_back({tick, std::move(name), std::move(args)});
}

void MatchRecord::note_first_contact(size_t faction, uint32_t tick) {
    if (recording_ && faction < first_contact_.size() && !first_contact_[faction]) first_contact_[faction] = tick;
}

void MatchRecord::finish(uint32_t tick, int result, float seconds) {
    if (!recording_) return;
    recording_ = false;
    end_tick_ = tick;
    result_ = result;
    seconds_ = seconds;
}

std::string MatchRecord::to_json() const {
    std::ostringstream out;
    out << "{\n  \"format\": \"mandate-of-war-match-record\",\n  \"format_version\": " << kFormatVersion;
    out << ",\n  \"info\": {";
    bool first = true;
    for (const auto& [key, value] : info_) {
        out << (first ? "\n    " : ",\n    ");
        write_string(out, key);
        out << ": ";
        write_string(out, value);
        first = false;
    }
    out << (info_.empty() ? "}" : "\n  }");
    out << ",\n  \"start_tick\": " << start_tick_ << ",\n  \"end_tick\": " << end_tick_;
    out << ",\n  \"finished\": " << (started_ && !recording_ ? "true" : "false");
    out << ",\n  \"result\": " << result_ << ",\n  \"match_seconds\": ";
    write_number(out, recording_ ? 0.0 : seconds_);
    out << ",\n  \"first_contact_tick\": [";
    for (size_t faction = 0; faction < first_contact_.size(); ++faction) {
        if (faction) out << ", ";
        if (first_contact_[faction]) out << *first_contact_[faction];
        else out << "null";
    }
    out << "],\n  \"command_position_scale\": ";
    write_number(out, command_position_scale_);
    out << ",\n  \"command_fields\": [\"tick\", \"faction\", \"type\", \"entity\", \"x\", \"y\", \"extra\"]";
    out << ",\n  \"commands\": [";
    for (size_t index = 0; index < commands_.size(); ++index) {
        const auto& command = commands_[index];
        out << (index ? ",\n    [" : "\n    [") << command.tick_id << ", " << static_cast<int>(command.player_id) << ", "
            << static_cast<int>(command.cmd_type) << ", " << command.entity_id << ", " << command.target_x << ", "
            << command.target_y << ", " << command.extra << "]";
    }
    out << (commands_.empty() ? "]" : "\n  ]");
    out << ",\n  \"actions\": [";
    for (size_t index = 0; index < actions_.size(); ++index) {
        const auto& action = actions_[index];
        out << (index ? ",\n    {" : "\n    {") << "\"tick\": " << action.tick << ", \"name\": ";
        write_string(out, action.name);
        out << ", \"args\": [";
        for (size_t arg = 0; arg < action.args.size(); ++arg) {
            if (arg) out << ", ";
            write_number(out, action.args[arg]);
        }
        out << "]}";
    }
    out << (actions_.empty() ? "]" : "\n  ]");
    out << ",\n  \"state_hashes\": [";
    for (size_t index = 0; index < hashes_.size(); ++index) {
        char hash[24];
        std::snprintf(hash, sizeof(hash), "%016" PRIx64, hashes_[index].second);
        out << (index ? ", " : "") << "[" << hashes_[index].first << ", \"" << hash << "\"]";
    }
    out << "]";
    out << ",\n  \"sample_fields\": [\"units\", \"fighters\", \"materials\", \"army_x\", \"army_y\"]";
    out << ",\n  \"samples\": [";
    for (size_t index = 0; index < samples_.size(); ++index) {
        const auto& sample = samples_[index];
        out << (index ? ",\n    {" : "\n    {") << "\"tick\": " << sample.tick << ", \"factions\": [";
        for (size_t faction = 0; faction < sample.factions.size(); ++faction) {
            const auto& summary = sample.factions[faction];
            out << (faction ? ", [" : "[") << summary.units << ", " << summary.fighters << ", ";
            write_number(out, summary.materials);
            out << ", ";
            write_number(out, summary.army_x);
            out << ", ";
            write_number(out, summary.army_y);
            out << "]";
        }
        out << "]}";
    }
    out << (samples_.empty() ? "]" : "\n  ]");
    out << "\n}\n";
    return out.str();
}

bool MatchRecord::commands_from_json(const std::string& json, std::vector<InputCommand>& commands) {
    commands.clear();
    const auto parsed = data::JsonParser::parse(json);
    if (!parsed || parsed->type() != data::JsonValue::Type::Object) return false;
    const auto version = parsed->get("format_version");
    const auto list = parsed->get("commands");
    if (!version || version->type() != data::JsonValue::Type::Number || version->as_number() != kFormatVersion ||
        !list || list->type() != data::JsonValue::Type::Array) return false;
    std::vector<InputCommand> read;
    for (const auto& entry : list->as_array()) {
        if (entry.type() != data::JsonValue::Type::Array || entry.array_size() != 7) return false;
        double fields[7];
        for (size_t field = 0; field < 7; ++field) {
            const auto& value = entry.as_array()[field];
            if (value.type() != data::JsonValue::Type::Number || !std::isfinite(value.as_number())) return false;
            fields[field] = value.as_number();
        }
        if (fields[0] < 0 || fields[1] < 0 || fields[1] > 255 || fields[2] < 0 || fields[2] > 255 || fields[3] < 0 ||
            fields[4] < -32768 || fields[4] > 32767 || fields[5] < -32768 || fields[5] > 32767 || fields[6] < 0) return false;
        InputCommand command{};
        command.tick_id = static_cast<uint32_t>(fields[0]);
        command.player_id = static_cast<uint8_t>(fields[1]);
        command.cmd_type = static_cast<uint8_t>(fields[2]);
        command.entity_id = static_cast<EntityId>(fields[3]);
        command.target_x = static_cast<int16_t>(fields[4]);
        command.target_y = static_cast<int16_t>(fields[5]);
        command.extra = static_cast<uint32_t>(fields[6]);
        read.push_back(command);
    }
    commands = std::move(read);
    return true;
}

} // namespace rts
