// SpeedRegulator.cpp - game-thread helicopter speed regulation.
// Runs inside the OnDriving prologue hook. Below MinApplySpeed vanilla
// logic fully controls hovering/stopping/attacks/exits. Suspended (and
// ramped back in) by the attitude-stability gate.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "Systems.h"
#include "AiCore.h"
#include "../Config/Config.h"
#include "../Game/HeliState.h"
#include "../Core/FrameTime.h"
#include "../Core/Log.h"

namespace Systems {

    namespace {
        void* gLastOwner = nullptr;
        float gPrevDirX = 0.0f, gPrevDirZ = 0.0f;
        float gTurnAmount = 0.0f;
        unsigned long gLastLogTick = 0;

        void ResetTurnState(void* owner) {
            gLastOwner = owner;
            gPrevDirX = gPrevDirZ = gTurnAmount = 0.0f;
        }
    }

    void __cdecl SpeedRegulatorTick(void* heliThis) {
        if (!gCfg.EnableSpeedRegulator) return;

        // Shared frame clock: 0 means this frame sits between fixed steps.
        const float dt = FrameTime::Step();
        if (dt <= 0.0f) return;

        HeliState::Snapshot s;
        if (!HeliState::Capture(heliThis, &s)) return;   // HeliState logs rejects
        if (s.owner != gLastOwner) ResetTurnState(s.owner);

        const char* gateReason = "full";
        const float assist = SpeedAssistScale(&gateReason);
        if (assist <= 0.0f) {
            gTurnAmount *= FrameTime::Damping(0.9f, dt);
            return;
        }

        if (gCfg.SkipDuringSkid && s.skidActive) return;
        if (s.driveSpeed < 0.0f) return;

        float target = s.driveSpeed * gCfg.SpeedFactor;
        if (gCfg.SpeedMax > 0.0f && target > gCfg.SpeedMax) target = gCfg.SpeedMax;

        const float vx = s.vel[0];
        const float vz = s.vel[2];
        const float currentSpeed = std::sqrt(vx * vx + vz * vz);

        if (target < gCfg.MinApplySpeed || currentSpeed < gCfg.MinApplySpeed) {
            gTurnAmount *= FrameTime::Damping(0.9f, dt);
            return;
        }
        if (currentSpeed < 2.0f) return;

        const float dirX = vx / currentSpeed;
        const float dirZ = vz / currentSpeed;
        if (gPrevDirX != 0.0f || gPrevDirZ != 0.0f) {
            float cross = gPrevDirX * dirZ - gPrevDirZ * dirX;
            if (cross < 0.0f) cross = -cross;
            // Per-frame blend authored at the reference rate. The turn amount
            // itself is a per-frame heading delta, so it is converted to a
            // reference-frame equivalent before being blended in.
            const float blend = FrameTime::Fraction(0.08f, dt);
            const float crossPerRefFrame = (dt > 0.0f)
                ? cross * (FrameTime::ReferenceStep() / dt) : cross;
            gTurnAmount += (crossPerRefFrame - gTurnAmount) * blend;
        }
        gPrevDirX = dirX;
        gPrevDirZ = dirZ;

        float turnScale = 1.0f;
        if (gCfg.TurnSlowdown) {
            turnScale = 1.0f / (1.0f + gTurnAmount * gCfg.TurnSlowdownStrength);
            if (turnScale < 0.25f) turnScale = 0.25f;
        }

        const float adjustedTarget = target * turnScale;
        const float delta = adjustedTarget - currentSpeed;
        // SpeedPush / OverspeedBrake are "fraction of the remaining gap per
        // frame" values authored at the reference rate; without conversion the
        // regulator would close that gap N times faster at N times the frame
        // rate and the helicopter would sit welded to its target speed.
        float rate = FrameTime::Fraction((delta >= 0.0f) ? gCfg.SpeedPush
                                                         : gCfg.OverspeedBrake, dt);
        rate *= assist;
        if (rate <= 0.0f) return;
        if (rate > 1.0f) rate = 1.0f;

        float newSpeed = currentSpeed + delta * rate;
        if (gCfg.SpeedMax > 0.0f && newSpeed > gCfg.SpeedMax) newSpeed = gCfg.SpeedMax;
        if (newSpeed < 0.0f) newSpeed = 0.0f;

        const float scale = newSpeed / currentSpeed;
        HeliState::WriteHorizontalVelocity(s, vx * scale, vz * scale);

        if (Log::IsVerbose()) {
            const unsigned long now = GetTickCount();
            if (now - gLastLogTick >= static_cast<unsigned long>(gCfg.VerboseIntervalMs)) {
                gLastLogTick = now;
                Log::Verbose("[ChopperSpeed] drive=%.1f cur=%.1f target=%.1f adj=%.1f new=%.1f "
                             "turn=%.3f assist=%.2f(%s)",
                             s.driveSpeed, currentSpeed, target, adjustedTarget, newSpeed,
                             gTurnAmount, assist, gateReason);
            }
        }
    }

} // namespace Systems
