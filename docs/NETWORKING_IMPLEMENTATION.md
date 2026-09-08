# Near-Future RTS — Networking Implementation

**Last updated:** 2026-09-06

**Milestone:** Goal 06 — Multiplayer, Replays, Stats, AI

**Reference:** `06_MULTIPLAYER_REPLAYS_STATS_AI.md`, `docs/NETWORKING.md`

## Current State

Networking infrastructure exists but is incomplete:

- `src/network/types.hpp`: Command, snapshot, and buffer type definitions
- `src/network/serializer.hpp`: Serialization functions for input commands, snapshots, delta snapshots
- `src/network/serializer.cpp`: Implementation of serialization functions
- `src/network/buffer.hpp`: Circular buffer for commands and snapshots
- `src/network/buffer.cpp`: Buffer implementation
- `src/network/network_manager.hpp`: NetworkManager class interface
- `src/network/network_manager.cpp`: NetworkManager implementation
- `src/network/portable_snapshot.cpp`: ADR-007 portable snapshot integration

All tests pass (106/106 integration tests).

## Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| Command serialization | ✅ VERIFIED | `serialize_input_command()`, `deserialize_input_command()` tested |
| Snapshot serialization | ✅ VERIFIED | `serialize_snapshot()`, `deserialize_snapshot()` tested |
| Buffer operations | ✅ VERIFIED | InputBuffer, SnapshotBuffer tested |
| NetworkManager | ✅ IMPLEMENTED | Header+cpp files present; no LAN transport layer |
| LAN discovery | ⏸️ TODO | UDP broadcast/multicast not yet implemented |
| Transport layer | ⏸️ TODO | TCP connection not yet implemented |
| Snapshot checksums | ⏸️ TODO | Determinism checkpoints not yet integrated |

## Next Steps

1. **LAN Discovery Protocol:**
   - Implement UDP broadcast discovery (500ms interval)
   - Include: server name, map hash, player count, max players, version
   - Join handshake: `JoinRequest` → `JoinResponse`

2. **Transport Layer:**
   - TCP connection establishment
   - `ConnectionHandshake` (client/server version, map hash, content versions)
   - `FrameCommandBatch` (tick, command count, packed commands per source player)
   - `SnapshotChecksum` (tick, snapshot hash, determinism validation)

3. **Integration:**
   - Connect NetworkManager with existing `CommandManager`
   - Add snapshot checksum exchange at determinism checkpoints
   - Test with 2 local instances playing identical command sequence

4. **Tests:**
   - Network command serialization/deserialization
   - Deterministic command application
   - State checksum verification
   - LAN discovery round-trip
   - Replay recording and playback

## References

- `docs/NETWORKING.md`: Full networking architecture spec
- `src/network/serializer.cpp`: Serialization implementation
- `docs/AI_ARCHITECTURE.md`: AI layer hierarchy
- `docs/REPLAY_FORMAT.md`: Replay file structure
