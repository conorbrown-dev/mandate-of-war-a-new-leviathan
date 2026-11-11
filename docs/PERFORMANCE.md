# Native intel detection evidence — 2026-11-08

Release build, macOS arm64, Apple M1 Pro (10 physical cores), 32 GiB RAM.
Commands, run sequentially after `cmake --build build --parallel 8`:

```sh
./build/rts_intel_benchmark 0
./build/rts_intel_benchmark 1
```

The isolated workload has 1,000 authored ground units (500 per side), all moving
on parallel routes, 100 fixed 50 ms ticks, vision enabled and AI disabled. Enemies
are beyond visual/weapon range but inside some intel coverage. Mode 0 disables
only intel range; mode 1 retains authored ranges. Assertions require at least
900 units moving and contacts present only with intel enabled. All 1,000 moved.

| Intel | Average native tick | P95 | Maximum | Contacts observed |
|---|---:|---:|---:|---|
| Off | 1.171 ms | 1.787 ms | 2.223 ms | No |
| On | 1.515 ms | 3.013 ms | 3.724 ms | Yes |

Both runs ended with hash `16625136552621004743`: radar alone did not turn this
non-engagement into a fight. This is one comparative run, not a scaling envelope.
Timing includes native simulation/vision/intel work, not Godot rendering. It
makes no FPS claim and does not measure the cost of rendering range envelopes.
Benchmark source: `benchmark/intel_benchmark.cpp`.


## Slice 4 engine allocator microbenchmark — 2026-11-09

Apple M1 Pro, 10 physical cores, 32 GiB RAM, macOS arm64; Release native library,
Godot 4.7.2 headless/Dummy audio. Command:

```
python3 tools/validate.py slice4_audio --timeout 45
```

The assertion-backed fixture separately exercises real native moving units and
an airborne paid paradrop transport. Its allocation timing feeds 1,000 already
computed audible candidates into `engine_audio.apply_candidates` for 20 passes.
It asserts exactly eight persistent voice nodes, selection of the nearest units,
stable ownership and cleanup at match end. Final full-sweep mean allocation time:
1.863 ms per call (eight voices). Source selection runs at most 10 Hz in game.

This is only a nearest-voice allocation microbenchmark. It excludes native
simulation, snapshot/pose collection, rendering, full audio mixing and a moving
1,000-unit rendered workload. It is not a game FPS or whole audio-frame budget
claim. The 23.50-second audio reel separately verifies non-silent segments and
no PCM clipping, not CPU scaling or subjective sound quality.


## Crowded combat and single-player snapshot follow-up — 2026-11-09

Apple M1 Pro, macOS arm64, 10 physical cores, 32 GiB RAM; Release build from
`cmake --build build --parallel 8`. Native commands:

```
./build/rts_combat_benchmark 2000 100
python3 tools/validate.py combat_benchmark_2000 --timeout 60
./build/rts_skirmish_benchmark
```

The combat fixture starts 2,000 units per faction in a dense active fight and
samples 100 fixed 50 ms ticks. It asserts combat, projectiles, destruction and
a native average below 12 ms. Before this follow-up, with ground separation
restored but no crowd bound or event-audience cache, the average was 33.197 ms
and p95 was 74.903 ms. After gating automatic network snapshots to multiplayer,
checking event ownership first, caching combat observers without fog and bounding
crowded-cell separation work to 64 candidates per body, the final validation
reports 9.001 ms average, 8.711 ms p50, 15.010 ms p95 and 25.172 ms maximum.
Its 4,000 initial units yielded 3,497 survivors and 6,270 fired projectiles;
initial/final state hashes were `4565373831983786770` and
`11650110860465471671`. The output is in
`validation/artifacts/combat_benchmark_2000/20261110T025226.717970Z/`.

The separate 1,000-unit, 400-tick active skirmish fixture includes movement,
180 projectile-active ticks, 544 destroyed units and one production build. It
reported 0.617 ms average, 1.856 ms p95 and 7.303 ms maximum. The workloads
are isolated native timing, not a Godot rendering, FPS or full-match pacing
claim. The combat fixture has no fog; the skirmish fixture is smaller and is
not a 4,000-unit rendered proxy. Human play still needs to assess frame pacing
when enemies appear and the effect materials first warm up.

### Crowded vehicles drive aside — 2026-11-10

Same machine and build. Ground-unit separation was reworked so an overlapped
stopped vehicle drives to a free spot and back instead of being slid (see
"Ground-unit separation" in STATUS.md). That adds a search for free spots, so
the cost was measured against the push-only version above.

- `combat_benchmark_2000` (4,000 tanks packed into two 20 m squares, the worst
  case for this: every tick thousands of overlapped stopped units): 10.6 ms
  average, 17.2 ms p95, 37.6 ms maximum, against the 12 ms gate. The push-only
  version was 6.3 ms. It passes with about 1.4 ms to spare, and runs were taken
  while other sessions used the machine, so treat it as marginal.
- `rts_skirmish_benchmark 1950 400`: 4.0 ms average, 12.5 ms p95, 20.7 ms
  maximum, against 2.8 ms push-only.
- `rts_scale_benchmark 1000 100 100`: 0.39 ms average, unchanged.

A first version of the search scanned every reserved spot linearly for every
candidate and averaged 17 ms on the skirmish fixture; reserving spots in the same
kind of grid as the hulls, one request per unit, no trigonometry per candidate and
an 8-tick backoff for packed crowds brought it to the numbers above. These are
isolated native timings, not a rendered frame-rate claim.

## Radio callouts (2026-11-10, Apple M1 Pro, Release)

Simulation and presentation measured separately; neither is a frame-rate claim.

- Simulation: `./build/rts_skirmish_benchmark 1950 400` (3,951 units) averages
  4.00 ms per tick (p95 12.8, max 20.5), unchanged from the 4.0 ms recorded
  before radio reports. The air check runs on the vision refresh over aircraft
  only; depot reports are raised only where a capture completes. A native test
  holds 1,500 hostile aircraft in sight for 200 ticks and gets one report.
- Presentation (headless Godot, GDScript): the announcer takes 7.9 ms to accept
  one packet of 2,000 synthetic reports and ends with at most six queued. The
  bridge can deliver at most 64 per side, so a real packet costs about 0.25 ms.
  An idle or speaking announcer costs about 2 microseconds a frame.
