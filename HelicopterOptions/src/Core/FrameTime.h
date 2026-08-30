// FrameTime.h - high-resolution frame clock and fixed-step gate.
//
// WHY THIS EXISTS
// The game's helicopter update runs once per rendered frame. With the frame
// rate capped at 60 (WidescreenFix SimRate = 0) that is the rate everything
// was tuned against. With the cap removed (SimRate = -1) the same update runs
// 2-6x more often, and two separate problems appear:
//
//  1. TIMING RESOLUTION. GetTickCount only advances every ~15.6 ms. At 240 FPS
//     (4.2 ms frames) three out of four updates measured a delta of zero, which
//     the AI layer treated as a fault: it skipped the update and re-armed the
//     estimator freeze. The target-motion estimate never became valid, so
//     prediction, stopped-player handling and direction-change recovery never
//     engaged - the helicopter simply held station. This module measures with
//     QueryPerformanceCounter instead, which resolves sub-microsecond deltas.
//
//  2. PER-FRAME MATH. Steps written as "move a fraction of the way each frame"
//     converge N times faster when the frame rate is N times higher, and the
//     engine's motion filters lose exactly the lag that produces the
//     helicopter's sweeping, swaying movement. Fraction()/Damping()/Linear()
//     convert a quantity authored per 60 FPS frame into the equivalent for the
//     timestep actually being used.
//
// TWO MODES
//   [FrameRate] Enable = 1   Above the reference rate the helicopter logic is
//                            gated to whole reference-rate steps: it runs with
//                            a fixed 1/60 s timestep and is skipped in between.
//                            This reproduces the 60 FPS behaviour exactly,
//                            including the overshoot that reads as sway, at any
//                            higher frame rate (75, 120, 144, 240, 360...).
//   [FrameRate] Enable = 0   No timing compensation at all: the mod keeps its
//                            hands off, and above 200 FPS the engine's own
//                            motion-derivation cutoff stiffens the helicopter.
//
// At or below the reference rate both modes behave identically to before.
#pragma once

namespace FrameTime {

    void Init();     // resolve the performance counter; call once at startup
    void Reset();    // forget the last timestamp (new helicopter, level load)

    // Called exactly ONCE per game frame, by the hook dispatcher, before any
    // tick consumer runs. Measures the real delta and decides whether this
    // frame carries a logic step.
    void BeginFrame();

    // Timestep for this frame, in seconds. Returns 0.0f when the frame falls
    // between fixed steps and the caller should do nothing at all - this is
    // normal pacing, NOT a fault, so callers must not treat it as an error.
    float Step();

    float RealDelta();      // measured frame time, seconds (raw, jittery)
    float SmoothedFps();    // low-passed, for logging
    float ReferenceStep();  // 1 / HelicopterUpdateRate
    bool  GateActive();     // legacy gating currently in effect
    unsigned SkippedFrames();

    // Low-passed frame time, clamped to a sane band. Use this - never the raw
    // delta - for anything that sets a TIME CONSTANT. Measured frame times on
    // an uncapped game swing wildly (0.5 ms to 17 ms between updates at a
    // nominal 240 FPS); feeding that jitter straight into a filter's constants
    // makes the filter thrash between heavily damped and barely damped, which
    // reads in-game as movement that sways and then abruptly stiffens.
    float SmoothedDelta();

    // The engine's OWN delta time, observed for free from the helicopter's
    // fuel countdown (it decrements by exactly the frame delta each update).
    // Purely diagnostic: it tells us whether the engine agrees with wall clock.
    void  NoteGameDelta(float dt);
    float GameDelta();

    // Convert quantities authored for one reference-rate frame to `dt`.
    // All three are exact identities when dt == ReferenceStep().
    //   Fraction: "move this fraction of the remaining distance each frame"
    //   Damping:  "multiply by this each frame" (0..1, retention)
    //   Linear:   "add this much each frame"
    float Fraction(float fractionPerRefFrame, float dt);
    float Damping (float retentionPerRefFrame, float dt);
    float Linear  (float deltaPerRefFrame, float dt);

    // Recompute an engine motion filter pair - output = (old*W + new) * S -
    // so its time constant matches the reference rate at the given timestep.
    // The stability invariant (W + 1) * S == 1 is preserved exactly.
    void ScaleFilterPair(float refWeight, float refScale, float dt,
                         float* outWeight, float* outScale);

} // namespace FrameTime
