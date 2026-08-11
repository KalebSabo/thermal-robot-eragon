# State Machine (ESP32 / Reflex)

- The goal of this note is to define what a **state machine** is for Eragon’s ESP32 Reflex layer.

## Definition

- A **state machine** is a way to organize firmware so the ESP32 is always in exactly **one known mode** (a *state*), and only moves to another mode when a defined *event* or *condition* happens.
- Instead of scattering `if` checks everywhere, behavior is split into:
    - **States** — what the robot is doing right now
    - **Transitions** — when and why it may leave that state
    - **Actions** — what to run while in a state, or when entering/leaving it

## Why it matters on the ESP32

- The Reflex layer owns low-level PWM, sensors, and link health (BLE / MQTT).
- A clear state machine keeps actuation predictable: the ESP32 does not try to walk, stand, and fault-recover in conflicting ways at the same time.
- FreeRTOS tasks (servo, serial/BLE, heartbeat) can share one current-state variable and react to the same transitions.

## Core pieces

| Piece | Meaning | Eragon example |
|-------|---------|----------------|
| State | Named mode the firmware is in | `standing`, `walking` |
| Event | Input that may cause a change | Serial `WALK` / `STAND`, MQTT command, fault |
| Transition | Allowed move from one state to another | `standing` → `walking` on `WALK` |
| Action | Work tied to a state or transition | Publish `eragon/gait/state`, update servo targets |

## Minimal gait states (current)

- Matches the MQTT gait publisher payloads on `eragon/gait/state`:

```
        WALK
  ┌──────────────┐
  │              ▼
standing ────► walking
  ▲              │
  └──────────────┘
        STAND
```

- **`standing`** — hold a stable pose; no gait cycle
- **`walking`** — run the gait / motion loop
- Only these transitions are valid for normal command flow; anything else is ignored or treated as a fault path later

## How to think about coding it

1. Define an enum for states (e.g. `STANDING`, `WALKING`).
2. Keep a single `currentState` (and optionally `previousState`).
3. On each loop or FreeRTOS tick: handle the **current** state’s actions, then check events for an allowed transition.
4. On transition: run exit action (old) → update state → run enter action (new), then publish the new gait state if the link is up.

## Related

- MQTT topics and payloads: `software/mqtt/notes.md`
- ESP32 Reflex notes: `firmware/notes.md`
