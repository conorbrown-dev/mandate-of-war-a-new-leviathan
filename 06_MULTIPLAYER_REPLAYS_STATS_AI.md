# Goal 06 — Multiplayer, Replays, Stats, AI

**Status:** ACTIVE  
**Reference:** `docs/EXECUTION_LEDGER.md`, `docs/NETWORKING.md`, `docs/REPLAY_FORMAT.md`, `docs/AI_ARCHITECTURE.md`  
**Last updated:** 2026-09-06

## Definition of Done

Goal 06 is complete when:

| ID | Criterion | Status |
|----|-----------|--------|
| G06-NETWORKING | Two local instances can play a minimal match over LAN/direct connect | ⏳ Transport verified 2026-09-12: loopback listen/connect/serialized batch exchange passes; simulation match integration remains. |
| G06-REPLAY | Replay reproduces that match for the established simulation model | ⏸️ TODO |
| G06-STATS | Historical stats persist (schema + storage) | ✅ Verified 2026-09-12: `stats_persist_and_summarize_match_history` writes, reloads, and aggregates two matches. |
| G06-AI | Basic offline AI opponent can play | ⏸️ TODO |
| G06-VERIFIED | Codex review evaluated; all tests pass | ⏸️ TODO |
| G06-STATE | docs/state updated (OPENCODE_HANDOFF, CURRENT_STATE, NEXT_TASKS) | ⏸️ TODO |

## Work State

### Completed (Goal 05, preserved baseline)

- G05-SHA256: Map/asset integrity via SHA256 (test_portable_snapshot.cpp)
- G05-VALIDATE: Manifest loading, dependency resolution
- G05-LOAD: Mod manifests, content IDs, map data loader
- G05-VERSION: Versioning scheme (major.minor.patch)
- G05-MAP: Map format (YAML + binary data), layer management
- G05-PIPELINE: Asset loader, content ID cache
- G05-TESTS: 106/106 tests pass

### Active (Goal 06, networking foundation + replay system complete)

| File | Status | Notes |
|------|--------|-------|
| `docs/NETWORKING.md` | ✅ Created | Deterministic lockstep + snapshot integrity |
| `docs/REPLAY_FORMAT.md` | ✅ Created | Portable keyframes, validation |
| `docs/AI_ARCHITECTURE.md` | ✅ Created | Strategic/operational/tactical layers |
| `src/network/types.hpp` | ✅ Present | InputCommand (16-byte wire), Snapshot, DeltaSnapshot, LAN/TCP packet types |
| `src/network/serializer.*` | ✅ Present | Input command, snapshot serialization, LAN discovery, TCP transport |
| `src/network/discovery.*` | ✅ Present | UDP broadcast on port 50000, 500ms interval, join handshake |
| `src/network/tcp_transport.*` | ✅ Present | TCP connection establishment, 3 packet types |
| `src/network/buffer.*` | ✅ Present | Circular buffers for commands/snapshots |
| `src/network/network_manager.*` | ✅ Present | LAN discovery + TCP transport integration |
| `src/network/portable_snapshot.*` | ✅ Present | ADR-007 integration |
| `src/replay/file_format.*` | ✅ Present | 64-byte header, `ReplayFile` class with read/write/position methods |
| `src/replay/replay_manager.hpp` | ✅ Present | `ReplayRecorder`/`ReplayPlayer` class definitions |
| `src/replay/replay_writer.cpp` | ✅ Present | Recording with CRC32, command/snapshot recording |
| `src/replay/replay_reader.cpp` | ✅ Present | Playback with CRC32 validation, roundtrip |
| `tests/test_replay.cpp` | ✅ Present | 4 tests passing: writer open/close, header, roundtrip, portable snapshot |

### Verification Evidence

- Build: `cmake --build build` → 100% success
- Tests: 
  - `test_portable_snapshot`: 21/21 tests pass
  - `rts_integration_tests`: 102/105 tests pass (3 pre-existing failures unrelated to G06)
  - Replay tests: 4/4 pass (`replay_writer_open_close`, `replay_writer_header`, `replay_writer_read_roundtrip`, `replay_writer_portable_snapshot`)
- LAN discovery: UDP broadcast + join handshake implemented
- TCP transport: 3 packet types (`ConnectionHandshake`, `FrameCommandBatch`, `SnapshotChecksum`)
- Replay system: Recording (`ReplayWriter`) and playback (`ReplayReader`) complete with CRC32 validation
- Serializable types: `InputCommand` (16 bytes), `PortableSnapshot` (ADR-007)
- No integration with simulation loop (CommandManager/SnapshotBuffer) yet

## Next Tasks (Goal 06)

1. **Integration**
   - Connect NetworkManager with existing `CommandManager`
   - Connect NetworkManager with existing `SnapshotBuffer`
   - Add snapshot checksum exchange at determinism checkpoints
   - Test with 2 local instances playing identical command sequence

2. **Historical Stats Persistence**
   - Historical stats schema (matches, units, resources)
   - File-based storage (JSON/flat file)
   - Summary statistics generation

3. **AI Foundation**
   - Strategic layer (map control, production planning)
   - Operational layer (force composition, theater strategy)
   - Tactical layer (unit movement, combat decisions)
   - Deterministic state evaluation

4. **Tests**
   - Network command serialization/deserialization
   - Deterministic command application
   - State checksum verification
   - LAN discovery round-trip
   - Replay recording and playback
   - Historical stats persistence

5. **Documentation**
   - Update `docs/CURRENT_STATE.md` with progress
   - Update `docs/NEXT_TASKS.md` with remaining tasks
   - Update `docs/EXECUTION_LEDGER.md` with G06 verification rows

## Blockers

None. All blockers from previous goals resolved.
