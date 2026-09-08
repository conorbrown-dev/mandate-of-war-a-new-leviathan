#pragma once

#include <vector>

#include "types.hpp"

namespace rts {

class InputBuffer {
public:
    explicit InputBuffer(size_t capacity = MAX_COMMANDS_PER_TICK * 2);
    
    void push(const InputCommand& cmd);
    bool pop(InputCommand& cmd);
    
    bool empty() const;
    bool full() const;
    size_t size() const;
    size_t capacity() const;
    
    void clear() {
        head_ = tail_ = 0;
    }

private:
    size_t capacity_;
    size_t head_;
    size_t tail_;
    std::vector<InputCommand> buffer_;
};

class SnapshotBuffer {
public:
    explicit SnapshotBuffer(size_t history_size = SNAPSHOT_HISTORY_SIZE);
    
    void store(const Snapshot& snapshot);
    bool get(uint32_t tick, Snapshot& out_snapshot) const;
    
    void store_delta(const DeltaSnapshot& snapshot);
    bool get_delta(uint32_t tick, DeltaSnapshot& out_snapshot) const;
    
    void store_legacy_buffer(const uint8_t* data, size_t size, uint32_t tick);
    bool get_legacy_buffer(uint32_t tick, uint8_t* out_buffer, size_t buffer_size, size_t& out_size) const;
    
    size_t size() const { return snapshot_count_; }
    
    void clear() {
        head_ = 0;
        delta_head_ = 0;
        snapshot_count_ = 0;
        delta_count_ = 0;
        legacy_buffer_head_ = 0;
        legacy_buffer_count_ = 0;
    }

private:
    size_t history_size_;
    size_t head_;
    size_t delta_head_;
    size_t legacy_buffer_head_;
    size_t snapshot_count_;
    size_t delta_count_;
    size_t legacy_buffer_count_;
    std::vector<Snapshot> history_;
    std::vector<DeltaSnapshot> delta_history_;
    std::vector<uint8_t> legacy_buffer_data_;
    std::vector<size_t> legacy_buffer_offsets_;
    std::vector<size_t> legacy_buffer_sizes_;
};

} // namespace rts
