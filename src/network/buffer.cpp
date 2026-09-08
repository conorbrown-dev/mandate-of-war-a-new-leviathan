#include "network/buffer.hpp"

#include <algorithm>
#include <cstring>

namespace rts {

InputBuffer::InputBuffer(size_t capacity) 
    : capacity_(std::max<size_t>(capacity, 2)), head_(0), tail_(0) {
    buffer_.resize(capacity_);
}

void InputBuffer::push(const InputCommand& cmd) {
    if (full()) {
        tail_ = (tail_ + 1) % capacity_;
    }
    buffer_[head_] = cmd;
    head_ = (head_ + 1) % capacity_;
}

bool InputBuffer::pop(InputCommand& cmd) {
    if (empty()) {
        return false;
    }
    cmd = buffer_[tail_];
    tail_ = (tail_ + 1) % capacity_;
    return true;
}

bool InputBuffer::empty() const {
    return head_ == tail_;
}

bool InputBuffer::full() const {
    return ((head_ + 1) % capacity_) == tail_;
}

size_t InputBuffer::size() const {
    if (head_ >= tail_) {
        return head_ - tail_;
    }
    return capacity_ - tail_ + head_;
}

size_t InputBuffer::capacity() const {
    return capacity_ - 1;
}

SnapshotBuffer::SnapshotBuffer(size_t history_size)
    : history_size_(std::max<size_t>(history_size, 1)),
      head_(0),
      delta_head_(0),
      legacy_buffer_head_(0),
      snapshot_count_(0),
      delta_count_(0),
      legacy_buffer_count_(0) {
    history_.resize(history_size_);
    delta_history_.resize(history_size_);
    legacy_buffer_offsets_.resize(history_size_);
    legacy_buffer_sizes_.resize(history_size_);
}

void SnapshotBuffer::store(const Snapshot& snapshot) {
    history_[head_] = snapshot;
    head_ = (head_ + 1) % history_size_;
    snapshot_count_ = std::min(snapshot_count_ + 1, history_size_);
}

bool SnapshotBuffer::get(uint32_t tick, Snapshot& out_snapshot) const {
    for (size_t i = 0; i < snapshot_count_; ++i) {
        if (history_[i].tick == tick) {
            out_snapshot = history_[i];
            return true;
        }
    }
    return false;
}

void SnapshotBuffer::store_delta(const DeltaSnapshot& snapshot) {
    delta_history_[delta_head_] = snapshot;
    delta_head_ = (delta_head_ + 1) % history_size_;
    delta_count_ = std::min(delta_count_ + 1, history_size_);
}

bool SnapshotBuffer::get_delta(uint32_t tick, DeltaSnapshot& out_snapshot) const {
    for (size_t i = 0; i < delta_count_; ++i) {
        if (delta_history_[i].tick == tick) {
            out_snapshot = delta_history_[i];
            return true;
        }
    }
    return false;
}

void SnapshotBuffer::store_legacy_buffer(const uint8_t* data, size_t size, uint32_t tick) {
    // Truncate old data when wrapping around the history
    if (legacy_buffer_count_ >= history_size_ && legacy_buffer_count_ > 0) {
        // Remove the oldest entry's data from legacy_buffer_data_
        size_t oldest_offset = legacy_buffer_offsets_[legacy_buffer_head_];
        size_t oldest_size = legacy_buffer_sizes_[legacy_buffer_head_];
        
        // Remove [oldest_offset, oldest_offset + oldest_size) from legacy_buffer_data_
        if (oldest_offset + oldest_size <= legacy_buffer_data_.size()) {
            legacy_buffer_data_.erase(
                legacy_buffer_data_.begin() + oldest_offset,
                legacy_buffer_data_.begin() + oldest_offset + oldest_size
            );
            
            // Adjust all offsets after the removed range
            for (auto& offset : legacy_buffer_offsets_) {
                if (offset > oldest_offset) {
                    offset -= oldest_size;
                }
            }
        }
    }
    
    size_t offset = legacy_buffer_data_.size();
    
    // Store tick at the beginning for lookup
    uint8_t tick_bytes[4] = {
        static_cast<uint8_t>(tick & 0xFF),
        static_cast<uint8_t>((tick >> 8) & 0xFF),
        static_cast<uint8_t>((tick >> 16) & 0xFF),
        static_cast<uint8_t>((tick >> 24) & 0xFF)
    };
    legacy_buffer_data_.insert(legacy_buffer_data_.end(), tick_bytes, tick_bytes + 4);
    
    // Store the portable snapshot data
    legacy_buffer_data_.insert(legacy_buffer_data_.end(), data, data + size);
    
    legacy_buffer_offsets_[legacy_buffer_head_] = offset;
    legacy_buffer_sizes_[legacy_buffer_head_] = size + 4;
    legacy_buffer_head_ = (legacy_buffer_head_ + 1) % history_size_;
    legacy_buffer_count_ = std::min(legacy_buffer_count_ + 1, history_size_);
}

bool SnapshotBuffer::get_legacy_buffer(uint32_t tick, uint8_t* out_buffer, size_t buffer_size, size_t& out_size) const {
    for (size_t i = 0; i < legacy_buffer_count_; ++i) {
        // Read tick from the first 4 bytes at offset
        uint32_t stored_tick = legacy_buffer_data_[legacy_buffer_offsets_[i]] |
                              (legacy_buffer_data_[legacy_buffer_offsets_[i] + 1] << 8) |
                              (legacy_buffer_data_[legacy_buffer_offsets_[i] + 2] << 16) |
                              (legacy_buffer_data_[legacy_buffer_offsets_[i] + 3] << 24);
        if (stored_tick == tick) {
            size_t size = legacy_buffer_sizes_[i] - 4;  // Exclude the 4-byte tick header
            if (buffer_size < size) {
                return false;
            }
            std::memcpy(out_buffer, legacy_buffer_data_.data() + legacy_buffer_offsets_[i] + 4, size);
            out_size = size;
            return true;
        }
    }
    return false;
}

} // namespace rts
