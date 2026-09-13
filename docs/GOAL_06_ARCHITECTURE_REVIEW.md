# Goal 06 Architecture Review

**Date:** 2026-09-12
**Scope:** G06-NETWORKING, G06-REPLAY, G06-STATS, and G06-AI acceptance evidence.

## Findings evaluated

1. **Direct-connect framing:** `TcpTransport::connect()` now assigns the
   active client socket used by frame send/receive operations. The focused
   loopback test establishes a real TCP connection, sends a `FrameCommandBatch`,
   injects the received authoritative command into the peer match, and asserts
   both command logs, state vectors, and checksums agree after the tick.

2. **Replay proof follows the networked command:** The same test advances the
   sending match to a legal terminal result, persists its replay, and requires
   `Skirmish::replay()` to reproduce every recorded checksum. This prevents the
   gate from relying only on isolated replay-file serialization.

3. **Offline opponent behavior:**
   `skirmish_validated_setup_and_repeatable_legal_terminal` runs paired,
   deterministic offline skirmishes to a legal terminal state. It asserts AI
   production and research completion rather than treating a tick loop or an
   empty AI update as proof.

4. **Stats storage:** `stats_persist_and_summarize_match_history` writes two
   records, reloads them, and verifies aggregate and per-map summaries. It uses
   a test-scoped temporary directory, leaving user match history untouched.

## Accepted boundary and follow-up

Goal 06 proves the required minimal direct-connect exchange and deterministic
replay, not a public multiplayer service: session UX, matchmaking, NAT
traversal, authentication, encryption, reconnection, and multi-peer
authoritative hosting are out of scope. The test is a real loopback socket test;
the normal sandbox cannot bind loopback ports, so the recorded release evidence
uses an approved unsandboxed local run. Those product-facing concerns must be
specified and validated by a later goal before release claims.

## Verdict

**ACCEPT.** The criteria in `06_MULTIPLAYER_REPLAYS_STATS_AI.md` have targeted
behavioral evidence and the full current Release/CTest/direct matrix passes.
