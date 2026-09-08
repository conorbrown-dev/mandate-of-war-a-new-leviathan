#include "network/types.hpp"

namespace rts {

struct NetworkMessage {
    NetworkHeader header;
    union {
        InputCommand command;
        Snapshot snapshot;
        uint32_t sequence_id;
    } payload;
};

} // namespace rts
