#pragma once

#include <vector>

#include "network/types.hpp"
#include "network/buffer.hpp"

namespace rts {

class NetworkManager;

class CommandManager {
public:
    explicit CommandManager(size_t input_buffer_capacity = MAX_COMMANDS_PER_TICK * 2);
    
    void set_network_manager(NetworkManager* manager);
    
    bool inject_local_command(const InputCommand& cmd);
    bool inject_local_commands(const std::vector<InputCommand>& commands);
    
    bool has_local_commands() const;
    size_t local_command_count() const;
    std::vector<InputCommand> get_local_commands() const;
    
    void flush_local_commands();
    void process_command(uint32_t player_id, uint8_t cmd_type, float target_x, float target_y, float extra_x, float extra_y, uint32_t extra);
    
    void clear();
    void clear_local_commands();

private:
    NetworkManager* network_manager_{nullptr};
    size_t input_buffer_capacity_;
    std::vector<InputCommand> local_command_queue_;
};

} // namespace rts
