#include "simulation/command_manager.hpp"
#include "network/network_manager.hpp"
#include "network/types.hpp"

namespace rts {

CommandManager::CommandManager(size_t input_buffer_capacity)
    : input_buffer_capacity_(input_buffer_capacity) {
}

void CommandManager::set_network_manager(NetworkManager* manager) {
    network_manager_ = manager;
}

bool CommandManager::inject_local_command(const InputCommand& cmd) {
    if (local_command_queue_.size() >= input_buffer_capacity_) {
        return false;
    }
    local_command_queue_.push_back(cmd);
    return true;
}

bool CommandManager::inject_local_commands(const std::vector<InputCommand>& commands) {
    if (commands.size() > input_buffer_capacity_ - local_command_queue_.size()) {
        return false;
    }
    local_command_queue_.insert(local_command_queue_.end(), commands.begin(), commands.end());
    return true;
}

bool CommandManager::has_local_commands() const {
    return !local_command_queue_.empty();
}

size_t CommandManager::local_command_count() const {
    return local_command_queue_.size();
}

std::vector<InputCommand> CommandManager::get_local_commands() const {
    return local_command_queue_;
}

void CommandManager::flush_local_commands() {
    if (network_manager_ && !local_command_queue_.empty()) {
        for (const auto& cmd : local_command_queue_) {
            network_manager_->send_command(cmd);
        }
        local_command_queue_.clear();
    }
}

void CommandManager::clear() {
    local_command_queue_.clear();
}

void CommandManager::clear_local_commands() {
    local_command_queue_.clear();
}

void CommandManager::process_command(uint32_t player_id, uint8_t cmd_type, float target_x, float target_y, float extra_x, float extra_y, uint32_t extra) {
    InputCommand cmd{};
    cmd.entity_id = static_cast<EntityId>(player_id);
    cmd.player_id = static_cast<uint8_t>(player_id);
    cmd.cmd_type = cmd_type;
    cmd.target_x = static_cast<int16_t>(target_x * INPUT_COMMAND_POSITION_SCALE);
    cmd.target_y = static_cast<int16_t>(target_y * INPUT_COMMAND_POSITION_SCALE);
    cmd.extra = extra;
    
    if (local_command_queue_.size() >= input_buffer_capacity_) {
        return;
    }
    
    local_command_queue_.push_back(cmd);
}

} // namespace rts
