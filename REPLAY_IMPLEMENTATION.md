# Replay System — Binary Format Implementation

## Overview

Implement replay record/playback using the binary format defined in `docs/REPLAY_FORMAT.md`.

## Format (from docs/REPLAY_FORMAT.md)

- **Magic**: 4 bytes (`'R', 'T', 'S', '0'`)
- **Version**: 1 byte (0x01)
- **CRC32**: 4 bytes (of whole file, excluding this field)
- **Metadata section**:
  - Map name length: 1 byte
  - Map name: N bytes
  - Match ID: 16 bytes (UUID)
  - Timestamp: 8 bytes (Unix epoch, big-endian)
  - Player count: 1 byte
  - Player IDs: 1 byte each
- **Initial state**: Full snapshot
- **Command log**: Variable-length commands (16 bytes each, packed)
- **Checksum verification**: CRC32 at end (or embedded in header)

## Implementation Plan

1. **ReplayWriter**: Record commands + initial state to binary file
2. **ReplayReader**: Playback from binary file
3. **Integration**: Hook into `Simulation` for recording, `RtsExtension` for playback
4. **Tests**: Record a match, replay it, compare state snapshots

## Files to Create/Modify

- `src/replay/replay_writer.hpp`, `src/replay/replay_writer.cpp`
- `src/replay/replay_reader.hpp`, `src/replay/replay_reader.cpp`
- `src/simulation/simulation.hpp`, `src/simulation/simulation.cpp` (recording hooks)
- `godot/gd_extension.cpp` (playback API)
- `tests/test_replay.cpp` (integration tests)

## Next Steps

1. Create `replay/` directory and implementations
2. Add ReplayWriter to `Simulation` (start/stop/record command)
3. Add ReplayReader for playback (forward, pause, seek)
4. Write integration tests (record → replay → compare snapshots)
5. Run tests, update `docs/CURRENT_STATE.md`
