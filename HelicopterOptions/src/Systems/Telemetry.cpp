// Telemetry.cpp - interval-throttled helicopter state logging + log rotation.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include "Systems.h"
#include "AiCore.h"
#include "HelicopterRegistry.h"
#include "../Radio/HeliRadioChat.h"
#include "../Config/Config.h"
#include "../Game/HeliState.h"
#include "../Core/Log.h"

namespace Systems {

    namespace {
        unsigned long gLastTick = 0;
        unsigned long gLastRotateCheck = 0;
        void* gLastOwner = nullptr;
    }

    void __cdecl TelemetryTick(void* heliThis) {
        Registry::Tick();

        const unsigned long now = GetTickCount();
        if (now - gLastRotateCheck >= 30000) {
            gLastRotateCheck = now;
            Log::CheckRotate(gCfg.MaxLogSizeKB);
        }

        if (!gCfg.EnableTelemetry) return;
        if (now - gLastTick < static_cast<unsigned long>(gCfg.TelemetryIntervalMs)) return;
        gLastTick = now;

        HeliState::Snapshot s;
        if (!HeliState::Capture(heliThis, &s)) return;   // HeliState logs rejects

        if (s.owner != gLastOwner) {
            gLastOwner = s.owner;
            Log::Info("Police helicopter detected and initialized.");
        }

        const float speed = std::sqrt(s.vel[0]*s.vel[0] + s.vel[2]*s.vel[2]);
        const float distToDest = std::sqrt(
            (s.dest[0]-s.pos[0])*(s.dest[0]-s.pos[0]) + (s.dest[2]-s.pos[2])*(s.dest[2]-s.pos[2]));
        static const char* kModeNames[4] = { "chase", "search", "attack", "attack2" };
        const char* modeName = (s.heliMode >= 0 && s.heliMode <= 3)
                             ? kModeNames[s.heliMode] : "n/a";
        Log::Info("[Diag:Heli] pos=(%.1f,%.1f,%.1f) spd=%.1f drive=%.1f "
                  "dest=(%.1f,%.1f,%.1f) dDest=%.1f above=%.1f fuel=%.1f skid=%d "
                  "mode=%s active=%d",
                  s.pos[0], s.pos[1], s.pos[2], speed, s.driveSpeed,
                  s.dest[0], s.dest[1], s.dest[2], distToDest, s.pos[1]-s.dest[1],
                  s.fuel, s.skidActive ? 1 : 0, modeName, Registry::AliveCount());

        if (gCfg.TelemetryAiState && AiCoreEnabled()) {
            AiDebug d{};
            GetAiDebug(&d);
            const char* assistReason = "full";
            const float assist = SpeedAssistScale(&assistReason);
            Log::Info("[Diag:AI] state=%s(%.1fs) tgtSpdEst=%.1f%s src=%s "
                      "physAng=%.2frad/s behavHead=%.0fdeg/s destHead=%.0fdeg/s "
                      "aggr=%.2f reattack=%.1f backoff=%.1f attacks=%d engaged=%d aborts=%d "
                      "unknown=%d stopped=%d reversal=%d faults=%d assist=%.2f(%s) "
                      "suppressed={trans=%u,pulses=%u} "
                      "lead={%.2f,%.1f,%.1f} entry={cd=%.1f,%.0f-%.0f,dot=%.2f}",
                      AiStateName(d.state), d.stateSeconds, d.targetSpeedEst,
                      d.targetEstFrozen ? "(estimated)" : "",
                      d.playerSource ? "player" : "estimate",
                      d.physicalAngSpeedRadSec, d.behavHeadingRateDegSec, d.destHeadingRateDegSec,
                      d.aggressionLevel, d.reattackTimer, d.backoffTimer,
                      d.attacksObserved, d.engagements, d.aborts, d.unclassified,
                      d.playerStopped ? 1 : 0, d.reversalDetected ? 1 : 0, d.faults,
                      assist, assistReason,
                      d.suppressedTransitions, d.suppressedAttackPulses,
                      gLive.LeadSpeedScale, gLive.LeadBase, gLive.LeadMax,
                      gLive.SkidCooldownThreshold, gLive.SkidEntryMinDistance,
                      gLive.SkidEntryMaxDistance, gLive.SkidEntryAlignmentDot);
            Log::Info("[Diag:Attack] active=%d elapsed=%.1f dist=%.1f minDist=%.1f "
                      "distValid=%d passed=%d attacks=%d engaged=%d aborts=%d unknown=%d "
                      "lastEnd=%s class=%s",
                      d.attackActive ? 1 : 0, d.attackElapsed, d.heliPlayerDistance,
                      d.minHeliPlayerDistance, d.distanceValid ? 1 : 0, d.passedTarget ? 1 : 0,
                      d.attacksObserved, d.engagements, d.aborts, d.unclassified,
                      d.lastEndReason, d.classificationReason);
            Log::Info("[Diag:Altitude] commandedHeight=%.1f actualAboveTarget=%.1f "
                      "vertVel=%.1f dynContrib=%.1f recovContrib=%.1f clamped=%d",
                      d.commandedHeight, d.actualHeightAboveTarget, d.verticalVelocity,
                      d.dynAltContribution, d.recoveryHeightContribution,
                      d.heightCommandClamped ? 1 : 0);
            if (d.attitudeValid) {
                Log::Info("[Diag:Attitude] upDot=%.2f roll=%.0f pitch=%.0f "
                          "physAngular=%.2frad/s stage=%d unstable=%.2fs",
                          d.upDot, d.rollDeg, d.pitchDeg, d.angSpeedRadSec,
                          d.attitudeStage, d.unstableSeconds);
            }
        }
    }

    bool AnyTickConsumerEnabled() {
        return gCfg.EnableSpeedRegulator || gCfg.EnableTelemetry || AiCoreEnabled()
            || Registry::Enabled() || HeliRadioChat::Enabled()
            || gCfg.IgnoreHeliSheet;
    }

} // namespace Systems
