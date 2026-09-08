# ADR-007: Portable Full-Snapshot Format

**Date:** 2026-08-29  
**Status:** Proposed

## Context

The current `Snapshot` serializer copies C++ structs directly. Its bytes depend on host byte order, padding, `bool` representation, and compiler layout, so it is suitable only for the current trusted in-process prototype. It is not a network, replay, or cross-platform persistence format.

Goal 02 needs a bounded, versioned representation of the complete state currently exposed by `Simulation::get_state()`: tick and sub-tick time, canonical active entity IDs, position, velocity, and health. This decision defines that representation before production serializers or wire types change.

## Decision

Introduce a new portable **full snapshot version 1** format. It is a standalone byte message, not a memory image. Every multi-byte value is encoded little-endian and every byte is assigned a meaning. Version 1 does not define delta snapshots, replay records, packet transport, compression, checksums, signatures, or peer authority.

The decoder accepts exactly one complete message. It either returns a fully validated value or fails without publishing partial state.

### Header: 32 bytes

| Offset | Size | Field | Version 1 rule |
|---:|---:|---|---|
| 0 | 4 | `magic` | ASCII `RTSS` (`52 54 53 53`) |
| 4 | 2 | `version` | Unsigned integer `1` |
| 6 | 1 | `message_kind` | `1` = full snapshot; every other value is rejected |
| 7 | 1 | `flags` | Must be zero |
| 8 | 2 | `header_bytes` | Must be `32` |
| 10 | 2 | `record_bytes` | Must be `40` |
| 12 | 4 | `total_bytes` | Must equal `32 + entity_count * 40` and the supplied buffer length |
| 16 | 4 | `tick` | Authoritative fixed-tick number |
| 20 | 4 | `subtick_ms` | IEEE-754 binary32 accumulator remainder, finite and in `[0, 50)` |
| 24 | 4 | `entity_count` | `0..100000` |
| 28 | 4 | `reserved` | Must be zero |

### Entity record: 40 bytes

| Record offset | Size | Field | Version 1 rule |
|---:|---:|---|---|
| 0 | 4 | `entity_id` | Active `uint32`; `0xFFFFFFFF` is invalid |
| 4 | 1 | `component_mask` | Must be `0x07`: position, velocity, and health are all present |
| 5 | 1 | `state_flags` | Bit 0 `ACTIVE` must be set; bit 1 mirrors `Health::is_dead`; all other bits must be zero |
| 6 | 2 | `reserved` | Must be zero |
| 8 | 4 | `position_x` | Canonical binary32 |
| 12 | 4 | `position_y` | Canonical binary32 |
| 16 | 4 | `position_z` | Canonical binary32 |
| 20 | 4 | `velocity_x` | Canonical binary32 |
| 24 | 4 | `velocity_y` | Canonical binary32 |
| 28 | 4 | `velocity_z` | Canonical binary32 |
| 32 | 4 | `health_current` | Canonical binary32 |
| 36 | 4 | `health_max` | Canonical binary32 |

Records must be strictly ascending by `entity_id`. Duplicate or out-of-order IDs are rejected. Entity IDs must be sorted before encoding; the encoder must sort the list of active entity IDs from the source state in ascending order before writing records. Version 1 represents active simulation entities that have all three required components; an encoder must fail rather than silently omit an active entity with missing required state.

### Integer and float encoding

- Unsigned integers use their fixed-width binary value in little-endian byte order.
- Float fields use the 32-bit IEEE-754 binary32 bit pattern, written as a little-endian `uint32` (for example, with `std::bit_cast<uint32_t>`).
- Encoders and decoders require `sizeof(float) == 4` and `std::numeric_limits<float>::is_iec559`; unsupported hosts fail at build time or report the format unsupported.
- All float fields must be finite. NaN and positive or negative infinity are rejected.
- Negative zero is normalized to positive zero before encoding, producing one canonical representation.
- Version 1 preserves valid non-zero finite binary32 values exactly and gives both signed zeros the canonical positive-zero encoding. It does not claim that floating-point simulation produces identical results across CPUs, compilers, or math-library settings.

### Semantic validation

In addition to structural validation:

- `subtick_ms` must be at least `0.0f` and less than the fixed 50 ms tick interval;
- position and velocity fields must be finite;
- `health_max` must be finite and non-negative;
- `health_current` must be finite and in `[0, health_max]`;
- `entity_id` must not equal `INVALID_ENTITY`;
- the component mask, state flags, and reserved fields must obey the exact version 1 rules above.

These checks define portable state validity, not gameplay authority. Future authoritative ingress must separately validate ownership, map bounds, legal transitions, tick windows, and per-peer resource limits.

### Length and failure rules

The maximum version 1 message is exactly `32 + 100000 * 40 = 4,000,032` bytes. This independently defined cap can represent the project's 25,000-unit practical target and 100,000-entity investigation ceiling; it does not inherit the legacy `Snapshot` struct's inadequate 1,024-entry fixed array. A future transport may choose a lower per-packet limit or chunk a validated payload, but it must not silently truncate a full snapshot.

Before reading any record, a decoder must:

1. verify that at least 32 bytes are available;
2. validate magic, version, kind, flags, fixed sizes, count, and reserved fields;
3. compute the expected size with checked arithmetic;
4. require `total_bytes` and the supplied buffer length to equal that expected size.

Truncated messages and messages with trailing bytes are rejected. A transport that carries multiple messages must frame them externally and pass exactly one declared frame to this decoder. Unknown versions are rejected; version negotiation belongs to a future handshake. Decode failure must leave the caller's output unchanged.

## Separation from Other Formats

- Existing `Snapshot`, `DeltaSnapshot`, and their raw-layout serializers remain provisional until replaced; this ADR does not make them portable.
- A portable delta format requires its own flags, reference-state semantics, quantization/error rules, and ADR. It must not reuse undocumented native delta structs.
- Replay files may embed a portable snapshot payload later, but replay headers, indexing, CRC coverage, and malformed-file behavior are a separate contract.
- Transport framing, compression, confidentiality, authentication, ordering, retransmission, and denial-of-service controls are outside this payload format.

## Required Implementation Tests

Before the portable serializer is accepted, automated tests must cover:

1. a byte-exact golden vector containing known integer and float values;
2. round trips for zero, one, 25,000, and 100,000 entities, plus rejection of 100,001;
3. canonical ascending entity order independent of source container iteration;
4. exact preservation of finite float bits and normalization of negative zero;
5. rejection of short headers, truncation at every field boundary, and trailing bytes;
6. rejection of bad magic, unknown versions/kinds, non-zero flags/reserved fields, wrong fixed sizes, impossible total lengths, and counts above 1,024;
7. rejection of duplicate/out-of-order/invalid IDs, invalid masks or state-flag bits, NaN/infinity, out-of-range sub-tick time, and invalid health ranges;
8. proof that failed decoding does not modify the output value;
9. tests that construct bytes explicitly rather than relying on the host layout of any C++ struct.

## Consequences

- Full snapshots have a predictable maximum payload and no padding disclosure.
- Golden vectors can be shared across Linux, Windows, and macOS implementations.
- Full snapshots remain relatively large and do not solve bandwidth, integrity, or hostile-peer concerns.
- Canonical bytes make later state hashing possible, but selecting and authenticating a hash is a separate decision.
- Cross-platform byte compatibility is achievable without claiming cross-platform simulation determinism.
