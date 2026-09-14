#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "FrameTime.h"
#include "Log.h"

namespace FrameTime {

    namespace {

        constexpr float kReferenceStep = 1.0f / 60.0f;
        constexpr float kLongestStep   = 0.25f;

        LARGE_INTEGER gFrequency = {};
        LARGE_INTEGER gLast = {};
        bool     gCounter = false;
        bool     gHaveLast = false;
        float    gStep = 0.0f;
        float    gAccumulated = 0.0f;
        float    gSmoothed = 0.0f;
        bool     gFixedSteps = false;
        unsigned gFrames = 0;
        float    gElapsed = 0.0f;
        bool     gReported = false;

        float Clamp(float value, float low, float high) {
            return value < low ? low : (value > high ? high : value);
        }

        float ReferenceFrames(float dt) {
            const float frames = dt / kReferenceStep;
            return std::isfinite(frames) ? Clamp(frames, 0.0f, 64.0f) : 1.0f;
        }

    }

    void Init() {
        gCounter = QueryPerformanceFrequency(&gFrequency) != 0 && gFrequency.QuadPart > 0;
        if (!gCounter)
            Log::Warn("No high-resolution timer is available; the helicopter is updated at a fixed 60 steps per second.");
        Reset();
    }

    void Reset() {
        gHaveLast = false;
        gStep = 0.0f;
        gAccumulated = 0.0f;
        gSmoothed = 0.0f;
        gFixedSteps = false;
    }

    void BeginFrame() {
        float dt = kReferenceStep;
        LARGE_INTEGER now;
        if (gCounter && QueryPerformanceCounter(&now)) {
            if (gHaveLast) {
                const double seconds = static_cast<double>(now.QuadPart - gLast.QuadPart)
                                     / static_cast<double>(gFrequency.QuadPart);
                if (seconds > 0.0 && seconds < 100.0) dt = static_cast<float>(seconds);
            }
            gLast = now;
            gHaveLast = true;
        }
        if (dt > kLongestStep) {
            dt = kLongestStep;
            gAccumulated = 0.0f;
        }

        const float outlier = kReferenceStep * 3.0f;
        if (gSmoothed <= 0.0f)
            gSmoothed = dt <= outlier ? dt : kReferenceStep;
        else if (dt <= outlier)
            gSmoothed += (dt - gSmoothed) * Clamp(dt / 0.25f, 0.0f, 1.0f);
        gSmoothed = Clamp(gSmoothed, kReferenceStep * 0.125f, kReferenceStep);

        const bool fixedSteps = dt < kReferenceStep;
        if (fixedSteps != gFixedSteps) {
            gFixedSteps = fixedSteps;
            gAccumulated = 0.0f;
        }

        if (!gFixedSteps) {
            gStep = dt;
        } else {
            gAccumulated += dt;
            if (gAccumulated + 1.0e-6f < kReferenceStep) {
                gStep = 0.0f;
            } else {
                gAccumulated -= kReferenceStep;
                if (gAccumulated > kReferenceStep) gAccumulated = kReferenceStep;
                gStep = kReferenceStep;
            }
        }

        ++gFrames;
        gElapsed += dt;
        if (!gReported && gElapsed >= 2.0f) {
            gReported = true;
            Log::Info("Helicopter updates arrive %.0f times per second; %s.", gFrames / gElapsed,
                      gFixedSteps ? "the mod runs its logic in fixed 60 Hz steps"
                                  : "no frame-rate compensation is needed");
        }
    }

    float Step() {
        return gStep;
    }

    float SmoothedDelta() {
        return gSmoothed > 0.0f ? gSmoothed : kReferenceStep;
    }

    void ScaleFilterPair(float weight, float scale, float dt, float* outWeight, float* outScale) {
        *outWeight = weight;
        *outScale = scale;

        const float retained = weight * scale;
        const float frames = ReferenceFrames(dt);
        if (!(retained > 0.0f) || retained >= 1.0f || frames <= 0.0f || frames == 1.0f) return;

        const float retainedNow = std::pow(retained, frames);
        if (!std::isfinite(retainedNow) || retainedNow <= 0.0f || retainedNow >= 0.999999f) return;

        const float newScale = 1.0f - retainedNow;
        *outScale = newScale;
        *outWeight = retainedNow / newScale;
    }

}
