# Security and Trust-Boundary Review

**Reviewed:** 2026-08-29

**Scope:** Native simulation, GDExtension bridge, local network serialization/buffers, provisional replay I/O, and architecture documents. This is a source review plus sanitizer/regression testing; it is not a dependency-CVE audit or multiplayer penetration test. There is currently no socket transport, account system, remote service, or secrets store.

## Current Trust Model

- Godot and the C++ simulation run in one local process and are trusted together.
- `NetworkManager` is an in-process prototype. Its packets, reliability counters, and snapshots must not be described as multiplayer security.
- Replay reader/writer files are provisional. Do not open replays from untrusted sources.
- Mods and content loading are design work only; no sandbox boundary exists.

## Corrected in the Goal 02 Recovery

1. Full and delta snapshot decoders reject counts above their fixed 1,024-entity storage before copying. The prior oversized full-snapshot input reproduced an AddressSanitizer stack overflow.
2. The 16-byte input-command wire encoding initializes every byte, zeros reserved bytes, and explicitly encodes integer fields little-endian.
3. Pathfinding rejects invalid dimensions, non-finite coordinates, and out-of-grid requests; the flow-field cache has a fixed 128-entry bound.
4. Unit creation rejects non-finite coordinates before spatial-grid conversion.
5. Simulation updates reject non-finite/non-positive deltas and cap one caller update to 250 ms, preventing an unbounded catch-up loop.
6. Formation and batched-position C entry points cap entity arrays at 100,000 and validate pointers/capacity used by the active GDExtension.

These are robustness foundations, not authorization or adversarial multiplayer support.

## Open Findings

### High — must resolve before accepting network or replay input

1. **Snapshots are still host-memory layouts in production.** ADR-007 now proposes a bounded version 1 full-snapshot schema with explicit little-endian fields, canonical finite binary32 values, exact-length parsing, canonical entity order, and semantic validation. It is not accepted or implemented yet. Full and delta snapshot serialization still copies C++ structs, floats, padding, and host byte order, so neither existing decoder is an untrusted-input boundary. Delta serialization remains undesigned.
2. **No peer security or authority exists.** There is no transport handshake, authentication, command ownership, tick-window validation, replay protection, rate limiting, server authority, desync recovery, or resource bound per peer. Do not attach the current buffers directly to UDP/TCP.
3. **Replay parsing is not an untrusted-file boundary.** CRC coverage is inconsistent, deterministic playback is absent, reads use exception-enabled streams without a complete top-level malformed-file contract, and several fields use native layout. The writer also accepts arbitrary output paths from its caller. Define a bounded, versioned format and fuzz malformed/truncated files before enabling replay sharing.

### Medium — constrain or replace before external reuse

4. **Legacy C wrappers rely on trusted callers.** Some provisional network and renderer wrappers dereference output or object pointers without null, alignment, or capacity information; `network_receive_delta_snapshot(uint32_t*)` is especially unsafe because the declared pointer type does not express a `DeltaSnapshot` buffer. Keep these symbols process-internal and replace them with typed span/capacity APIs before reuse.
5. **Commands are structurally decoded but not semantically validated.** `cmd_type`, player/entity ownership, target bounds, tick age, and command rate are unchecked. Semantic validation belongs at the future authoritative simulation ingress, not in transport-only decoding.
6. **The GDExtension has no capability model.** Any trusted project script can create/destroy/order arbitrary IDs. That is acceptable for the current single-process prototype, but a future mod/script system needs a narrower command API and explicit ownership.
7. **Dependency provenance is incomplete.** Root `.gitmodules` metadata is missing for gitlinks, generated/vendor outputs are mixed with source, and the GDExtension links a hard-coded godot-cpp debug archive. Reproducible dependency pinning and release builds are prerequisites for a supply-chain review.

## Required Security Gates

Before multiplayer, shared replay, or executable mod support:

1. write a threat model covering host, peer, lobby/discovery, replay, mod/content, and denial-of-service boundaries;
2. use explicit versioned wire/file schemas with checked arithmetic and bounded allocation;
3. enforce authoritative ownership, valid command enums/ranges/ticks, and per-peer quotas before state mutation;
4. add malformed/truncated/property tests and coverage-guided fuzzing for every parser;
5. authenticate peers or clearly constrain the feature to trusted LAN use, with no claim of hostile-network safety;
6. sandbox or declaratively constrain mods and content; never load arbitrary native libraries as data mods;
7. pin and reproduce third-party dependencies, then run dependency and binary hardening checks in CI.

For ADR-007 implementation specifically, the decoder must validate the complete 32-byte header before reading records, use checked length arithmetic, cap the message at 4,000,032 bytes, reject unknown enum/flag values and non-canonical entity ordering, reject truncation and trailing bytes, reject invalid numeric state, and leave output unchanged on every failure. Passing those parser tests will close only the portable full-snapshot portion of finding 1; it will not approve delta snapshots, transport, replay, or peer trust.

## Review Boundary

The active Goal 02 code can be tested locally with the current trusted Godot presentation. It is not approved for hostile network peers, untrusted replay files, or untrusted mods. Later milestone work must close the relevant findings rather than treating this document as proof that those features are secure.
