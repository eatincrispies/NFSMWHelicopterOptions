// FrameTime.cpp - high-resolution frame clock and fixed-step gate.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "FrameTime.h"
#include "Log.h"
#include "../Config/Config.h"

namespace FrameTime {

    namespace {

        LARGE_INTEGER gFreq = {};
        LARGE_INTEGER gLast = {};
        bool     gCounterOk = false;
        bool     gHaveLast  = false;

        float    gRealDt    = 0.0f;
        float    gStepDt    = 0.0f;
        float    gAccum     = 0.0f;
        float    gFpsSmooth = 0.0f;
        float    gSmoothDt  = 0.0f;   // low-passed frame time (time-constant work)
        float    gGameDt    = 0.0f;   // engine's own delta, from the fuel countdown
        bool     gGating    = false;
        unsigned gSkipped   = 0;

        unsigned long gLastLogMs = 0;
        bool     gModeLogged = false;

        // First-seconds report: answers "is this code even running, and what
        // rate does the helicopter update actually arrive at?" without needing
        // global verbose logging switched on.
        unsigned gFrames  = 0;      // frames the clock has seen
        unsigned gSteps   = 0;      // frames that carried a logic step
        float    gElapsed = 0.0f;
        bool     gReported = false;

        float Clampf(float v, float lo, float hi) {
            return v < lo ? lo : (v > hi ? hi : v);
        }
        bool Finite(float v) { return v == v && v * 0.0f == v * 0.0f; }

        float MaxStep() {
            // Longest delta we are willing to integrate in one go. Anything
            // longer is a stall (loading, alt-tab) and is clamped so a single
            // huge step cannot fling the helicopter.
            return kFrameMaxStepSeconds;
        }

    } // namespace

    void Init() {
        gCounterOk = (QueryPerformanceFrequency(&gFreq) != 0) && gFreq.QuadPart > 0;
        Reset();
        gModeLogged = false;
        if (!gCounterOk) {
            Log::Warn("This system has no high-resolution timer; the helicopter will be "
                      "updated at a fixed %.0f steps per second.", 1.0f / ReferenceStep());
        }
    }

    void Reset() {
        gHaveLast = false;
        gAccum    = 0.0f;
        gRealDt   = 0.0f;
        gStepDt   = 0.0f;
        gGating   = false;
        gSkipped  = 0;
        gSmoothDt = 0.0f;
        gGameDt   = 0.0f;
        gFrames   = 0;
        gSteps    = 0;
        gElapsed  = 0.0f;
        gReported = false;
    }

    float ReferenceStep() {
        const float fps = Clampf(gCfg.HelicopterUpdateRate, 20.0f, 120.0f);
        return 1.0f / fps;
    }

    void BeginFrame() {
        const float refStep = ReferenceStep();

        // ---- measure the real frame delta -------------------------------
        float dt = refStep;
        if (gCounterOk) {
            LARGE_INTEGER now;
            if (QueryPerformanceCounter(&now)) {
                if (!gHaveLast) {
                    gLast = now;
                    gHaveLast = true;
                    dt = refStep;             // first frame: assume nominal
                } else {
                    const double secs = static_cast<double>(now.QuadPart - gLast.QuadPart)
                                      / static_cast<double>(gFreq.QuadPart);
                    gLast = now;
                    dt = (secs > 0.0 && secs < 100.0) ? static_cast<float>(secs) : refStep;
                }
            }
        }
        if (!Finite(dt) || dt <= 0.0f) dt = refStep;

        const float maxStep = MaxStep();
        if (dt > maxStep) {
            dt = maxStep;
            gAccum = 0.0f;                    // stall: drop the backlog
        }
        gRealDt = dt;

        if (gFpsSmooth <= 0.0f) gFpsSmooth = 1.0f / dt;
        else                    gFpsSmooth += ((1.0f / dt) - gFpsSmooth) * 0.05f;

        // Low-passed delta with a ~0.25 s time constant. Clamped so a stall or
        // a burst of very short frames cannot drag a filter's time constant to
        // an extreme; below refStep/8 (eight times the reference rate) there is
        // nothing further to gain from smoothing harder.
        if (gSmoothDt <= 0.0f) {
            gSmoothDt = dt;
        } else {
            float k = dt / 0.25f;
            if (k > 1.0f) k = 1.0f;
            gSmoothDt += (dt - gSmoothDt) * k;
        }
        gSmoothDt = Clampf(gSmoothDt, refStep * 0.125f, maxStep);

        // ---- decide this frame's logic step ------------------------------
        // Gating only makes sense ABOVE the reference rate. At or below it the
        // real delta is used, exactly as the mod has always behaved.
        const bool wantGate = gCfg.EnableFrameRateFix && (dt < refStep);
        if (wantGate != gGating) {
            gGating = wantGate;
            gAccum  = 0.0f;
            if (gCfg.EnableFrameRateFix && !gModeLogged) {
                gModeLogged = true;
                Log::Info("Frame rate is above %.0f FPS (currently about %.0f). The "
                          "helicopter will be updated in fixed %.1f ms steps so it behaves "
                          "the same as it does at %.0f FPS.",
                          1.0f / refStep, gFpsSmooth, refStep * 1000.0f, 1.0f / refStep);
            }
        }

        if (!gGating) {
            gAccum  = 0.0f;
            gStepDt = dt;
        } else {
            gAccum += dt;
            if (gAccum + 1.0e-6f < refStep) {
                gStepDt = 0.0f;               // between steps: skip this frame
                ++gSkipped;
            } else {
                gAccum -= refStep;
                // Never let the backlog grow past a single step; a persistent
                // surplus would make the helicopter run fast to catch up.
                if (gAccum > refStep) gAccum = refStep;
                gStepDt = refStep;
            }
        }

        // ---- reporting ----------------------------------------------------
        // These use Info, not Verbose: LogFrameRate is a dedicated switch and
        // must not also require VerboseLogging in GeneralSettings.ini.
        ++gFrames;
        if (gStepDt > 0.0f) ++gSteps;
        gElapsed += dt;

        if (!gReported && gElapsed >= 2.0f) {
            gReported = true;
            Log::Info("Helicopter update rate: %u updates in %.1f s (%.0f per second, "
                      "steady %.2f ms apart, engine reports %.2f ms). Logic steps run: "
                      "%u (%.0f per second). Fixed-step mode is %s (reference %.0f FPS).",
                      gFrames, gElapsed, gFrames / gElapsed, gSmoothDt * 1000.0f,
                      gGameDt * 1000.0f, gSteps, gSteps / gElapsed,
                      gGating ? "ON" : "off", 1.0f / refStep);
            if (!gGating && gCfg.EnableFrameRateFix)
                Log::Info("Fixed-step mode is idle because the helicopter is being "
                          "updated at or below %.0f times per second. If the game is "
                          "running faster than that, its helicopter update is not tied "
                          "to the frame rate and this option cannot affect it.",
                          1.0f / refStep);
        }

        if (gCfg.LogFrameRate) {
            const unsigned long nowMs = GetTickCount();
            if (nowMs - gLastLogMs >= 5000) {
                gLastLogMs = nowMs;
                float fw = 0.0f, fs = 0.0f;
                ScaleFilterPair(7.0f, 0.125f, gSmoothDt, &fw, &fs);
                Log::Info("[FrameRate] %.0f/s raw %.2f ms, steady %.2f ms, engine %.2f ms, "
                          "step %.2f ms, fixed-step %s, smoothing W=%.1f S=%.4f, "
                          "%u stepped / %u skipped.",
                          gFpsSmooth, gRealDt * 1000.0f, gSmoothDt * 1000.0f,
                          gGameDt * 1000.0f, gStepDt * 1000.0f,
                          gGating ? "on" : "off", fw, fs, gSteps, gSkipped);
            }
        }
    }

    float Step()          { return gStepDt; }
    float RealDelta()     { return gRealDt; }
    float SmoothedDelta() { return gSmoothDt > 0.0f ? gSmoothDt : ReferenceStep(); }
    float GameDelta()     { return gGameDt; }

    void NoteGameDelta(float dt) {
        if (dt > 0.0f && dt < 1.0f && Finite(dt)) {
            if (gGameDt <= 0.0f) gGameDt = dt;
            else                 gGameDt += (dt - gGameDt) * 0.05f;
        }
    }
    float SmoothedFps()   { return gFpsSmooth; }
    bool  GateActive()    { return gGating; }
    unsigned SkippedFrames() { return gSkipped; }

    // ---------------------------------------------------------------- maths
    // n = how many reference-rate frames this timestep represents.
    static float Frames(float dt) {
        const float n = dt / ReferenceStep();
        return Finite(n) ? Clampf(n, 0.0f, 64.0f) : 1.0f;
    }

    float Fraction(float f, float dt) {
        if (!(f > 0.0f)) return 0.0f;
        if (f >= 1.0f)   return 1.0f;
        const float n = Frames(dt);
        if (n <= 0.0f) return 0.0f;
        if (n == 1.0f) return f;
        // Keep the same amount of "remaining distance" after dt seconds as
        // n applications of the per-frame fraction would leave.
        const float r = std::pow(1.0f - f, n);
        return Finite(r) ? Clampf(1.0f - r, 0.0f, 1.0f) : f;
    }

    float Damping(float d, float dt) {
        if (!(d > 0.0f)) return 0.0f;
        if (d >= 1.0f)   return 1.0f;
        const float n = Frames(dt);
        if (n == 1.0f) return d;
        const float r = std::pow(d, n);
        return Finite(r) ? Clampf(r, 0.0f, 1.0f) : d;
    }

    float Linear(float v, float dt) {
        const float r = v * Frames(dt);
        return Finite(r) ? r : v;
    }

    void ScaleFilterPair(float refWeight, float refScale, float dt,
                         float* outWeight, float* outScale) {
        if (!outWeight || !outScale) return;
        *outWeight = refWeight;
        *outScale  = refScale;
        if (!Finite(refWeight) || !Finite(refScale)) return;

        // The engine filter is  out = (old * W + new) * S, i.e. an EMA that
        // retains  a = W * S  of the previous value each frame. Holding the
        // time constant fixed means retaining  a^n  over n reference frames.
        const float a = refWeight * refScale;
        if (!(a > 0.0f) || a >= 1.0f) return;      // not a normalised pair
        const float n = Frames(dt);
        if (n <= 0.0f || n == 1.0f) return;

        const float aN = std::pow(a, n);
        if (!Finite(aN) || aN <= 0.0f || aN >= 0.999999f) return;

        // S' = 1 - a'  and  W' = a' / S'  gives  (W' + 1) * S' == 1 exactly,
        // so the anti-spin stability rule is satisfied by construction.
        const float s = 1.0f - aN;
        const float w = aN / s;
        if (!Finite(s) || !Finite(w) || s <= 0.0f || w < 0.0f) return;
        *outWeight = w;
        *outScale  = s;
    }

} // namespace FrameTime
