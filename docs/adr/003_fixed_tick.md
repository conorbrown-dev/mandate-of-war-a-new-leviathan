# ADR-003: Fixed-Tick Simulation

**Date:** 2026-08-24  
**Status:** Accepted

## Decision

Implement simulation with fixed 50ms tick (20 ticks/sec) independent of rendering frame rate.

The caller-facing accumulator rejects non-finite and non-positive deltas and retains at most 250 ms, or five ticks, from one update. This prevents an invalid or extremely large presentation delta from creating an unbounded catch-up loop. Time beyond that cap is deliberately dropped; future multiplayer/server timing must drive authoritative ticks explicitly rather than trusting client frame deltas.

## Rationale

**Variabletick problems:**
- Non-deterministic: Same input → different results
- Frame rate dependent: Fast GPU = faster simulation
- Difficult networking: interpolation becomes complex
- Unreliable for replays: timing differences

**Fixed tick benefits:**
- Deterministic: Same input → same output
- Consistent gameplay speed regardless of GPU
- Easier networking: synchronize tick state
- Reproducible replays

## Implementation

### Main Loop

```cpp
class Simulation {
    const float TICK_RATE = 50.0f;  // 20 ticks/sec
    
    void run() {
        auto last_time = std::chrono::steady_clock::now();
        auto tick_interval = std::chrono::milliseconds(TICK_RATE);
        
        while (running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - last_time;
            
            if (elapsed >= tick_interval) {
                tick();
                last_time = now;
            } else {
                // Render with interpolation
                render_interpolated();
                std::this_thread::sleep_for(1ms);
            }
        }
    }
    
    void tick() {
        input_phase();
        prediction_phase();
        combat_phase();
        environment_phase();
        output_phase();
    }
};
```

### Tick Phases

```
+------------+  (1ms)
|   Input    |  Apply player commands to command buffer
+------------+  
| Prediction |  (5ms)  Update positions, state
+------------+
|   Combat   |  (10ms) Resolve projectiles, damage
+------------+
| Environment|  (5ms)  Resources, terrain, effects
+------------+
|   Output   |  (1ms)  Serialize state for rendering
+------------+
Total: ~22ms/tick target
```

### Client-Server Model

**Headless server (deterministic):**
```cpp
// No interpolation needed, direct state
simulation.tick();
send_state_to_clients();
```

**Client (rendering):**
```cpp
// Interpolate between last two ticks
void render() {
    auto now = get_time();
    auto alpha = (now - last_tick_time) / TICK_RATE;
    render_interpolated(state, next_state, alpha);
}
```

## Networking Implications

### State Synchronization

Send snapshot every N ticks:
```
Client                    Server
  |                        |
  |---- Snapshot (tick 0)->
  |                        | (2 ticks later)
  |---- Snapshot (tick 2)->
  |                        | (2 ticks later)
  |---- Snapshot (tick 4)->
```

### Input Lag

Client sends input:
```
Client time 0: User clicks "move"
Client time 1: Input packet sent
Server time 50: Input received, queued
Server time 100: Input applied (next tick)
```

**Lag:** ~100ms from input to visible result

### Client-Side Prediction

```cpp
// Client predicts ahead, validates on receipt
void move_entity(EntityId entity, Vector3 target) {
    // Predict locally
    predict_move(entity, target);
    render_prediction();
    
    // Send to server
    send_command(entity, Command::MOVE, target);
}

// Server validation
void receive_command(Command cmd) {
    auto result = apply_command(cmd);
    if (result != prediction) {
        // Snap to server state
        snap_to_server_state();
    }
}
```

## Input Compression

### Delta Encoding

Instead of sending full state every tick:
```
Tick 0: Full state (position, velocity, etc)
Tick 1: Only changed components
Tick 2: Only changed components
```

### Keyframe + Deltas

```
Every 10 ticks: Full snapshot
Between: Delta packets (only changed fields)
```

## Performance Budget

| Phase | Target | Max |
|-------|--------|-----|
| Input | <1ms | 2ms |
| Prediction | <5ms | 10ms |
| Combat | <10ms | 20ms |
| Environment | <5ms | 10ms |
| Output | <1ms | 2ms |
| **Total** | **~22ms** | **46ms** |

## Validation

1. **Determinism test**: Run same scenario twice, verify identical output
2. **Timing test**: Measure tick-to-tick consistency
3. **Stress test**: 50k units, verify tick time under budget

## Future Enhancements

1. **Variable tick rate**: Adjust based on complexity (advanced)
2. **Sub-stepping**: Multiple small ticks for physics accuracy
3. **Time scale**: Fast-forward for testing, slow-motion for important moments
