#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include "AiCore.h"
#include "Systems.h"
#include "HelicopterRegistry.h"
#include "../Radio/HeliRadioChat.h"
#include "../Config/Config.h"
#include "../Game/HeliState.h"
#include "../Core/Addresses.h"
#include "../Core/FrameTime.h"
#include "../Core/Memory.h"
#include "../Core/Log.h"

namespace Systems {

    namespace {

        float Len2(float x, float z) { return std::sqrt(x * x + z * z); }
        float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
        float Lerp(float a, float b, float t) { return a + (b - a) * t; }
        float Dot3(const float* a, const float* b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
        void  Cross3(const float* a, const float* b, float* o) {
            o[0] = a[1]*b[2] - a[2]*b[1];
            o[1] = a[2]*b[0] - a[0]*b[2];
            o[2] = a[0]*b[1] - a[1]*b[0];
        }
        constexpr float kRad2Deg = 57.29578f;

        // ------------------------------------------------------------------ tracker
        struct TrackerState {
            void* ownerKey = nullptr;
            float fuelPrev = 0.0f;      // for observing the engine's own delta

            float destPrev[3] = {};
            float destVel[3] = {};
            float destVelOld[3] = {};
            float destVelOldTimer = 0.0f;
            float targetSpeed = 0.0f;
            float targetSpeedPrev = 0.0f;
            float targetAccel = 0.0f;
            // DestinationHeadingChangeRate: how fast the TARGET's velocity
            // direction rotates (deg/s). Behavioral, not physical.
            float destHeadingRateDegSec = 0.0f;
            float tgtDirPrev[2] = {};
            bool  histValid = false;
            float freezeTimer = 0.0f;
            int   validSamples = 0;

            float ownDirPrev[2] = {};
            // BehavioralHeadingChangeRate: how fast the HELICOPTER's velocity
            // direction rotates (deg/s). At low speed this can spike to
            // hundreds of deg/s from small strafes while the BODY barely
            // rotates - it is NOT physical rotation and must not be treated
            // as such. PhysicalAngularSpeed lives in AttitudeState.angSpeed
            // (rad/s, derived from orientation-vector deltas).
            float behavHeadingRateDegSec = 0.0f;
            float turnSignPrev = 0.0f;
            float flipRate = 0.0f;

            float lowSpeedTime = 0.0f;
            int   lowSpeedSamples = 0;
            int   fastSamples = 0;
            bool  stopped = false;

            bool  playerSrcActive = false;
            bool  srcLoggedOnce = false;
            unsigned long lastSrcSwitchLogMs = 0;

            // Latest verified player position (world), when the player source
            // is available this tick. Used for real heli-to-player distance.
            float playerPos[3] = {};
            bool  playerPosValid = false;

            float stablePathTime = 0.0f;
            float noProgressTime = 0.0f;
            float circlingTime = 0.0f;
            bool  reversal = false;
            bool  sharpTurn = false;
            bool  braking = false;
        };

        // ------------------------------------------------------------------ attacks
        // The raw skid observable pulses at game-tick frequency and does NOT
        // stay continuously true for a whole physical attack. Start detection
        // uses a pulse-tolerant EVIDENCE latch (positive samples accumulate,
        // a short window forgives brief false gaps). Lifecycle COMPLETION is
        // driven by the real helicopter-to-player distance (verified positions)
        // rather than by the raw flag dropping - so an attack no longer ends
        // after 0.2-0.3s and is no longer always classified as an abort.
        constexpr float kEvidenceWindowSeconds = 0.5f;
        constexpr float kMinAttackSeconds = 1.0f;     // logical attacks last at least this
        constexpr float kEngageDistance = 12.0f;      // <= this counts as a real engagement
        constexpr float kRawGapEndSeconds = 2.0f;     // skid quiet this long ends the attack
        struct AttackState {
            bool  active = false;
            int   id = 0;
            float lifeTime = 0.0f;
            float minDist = 0.0f;
            bool  minDistValid = false;
            float lastDist = 0.0f;
            bool  lastDistValid = false;
            bool  passedTarget = false;   // heli-player distance bottomed out then grew
            float risingTime = 0.0f;      // time distance has been increasing after the low
            float posEvidence = 0.0f;     // accumulated positive seconds (start)
            float windowTimer = 0.0f;     // gap-tolerance window remaining
            float timeSinceRawPulse = 0.0f; // quiet time since the last raw skid sample
            bool  rawPrev = false;
            unsigned rawPulses = 0;       // raw rising edges (diagnostics)
            int   acceptedEdges = 0;      // accepted attack starts
            const char* endReason = "";
            const char* classification = "";
            int   attacksObserved = 0;
            int   engagements = 0;
            int   aborts = 0;
            int   unclassified = 0;
            unsigned suppressedPulses = 0;
        };

        // ------------------------------------------------------------------ attitude
        struct AttitudeState {
            int   upOrder = 0;
            int   calibCount = 0;
            float calibSum = 0.0f;
            bool  unavailableLogged = false;

            float prevFwd[3] = {};
            float prevUp[3] = {};
            bool  prevValid = false;

            float upDot = 1.0f;
            float rollDeg = 0.0f;
            float pitchDeg = 0.0f;
            float angSpeed = 0.0f;

            float unstableTime = 0.0f;
            float stableTime = 0.0f;
            bool  active = false;
            int   stage = 0;
            float recoveryTime = 0.0f;
            float reenableTime = 1.0e6f;
            bool  valid = false;
        };


        // ------------------------------------------------------------------ machine
        struct MachineState {
            AiState state = AiState::Arrival;
            float   stateTime = 0.0f;
            float   aliveTime = 0.0f;

            float reattackTimer = 0.0f;
            float backoffTimer = 0.0f;
            float dirRecoveryTimer = 0.0f;
            float dirCooldownTimer = 0.0f;
            float circRecoveryTimer = 0.0f;
            float circCooldownTimer = 0.0f;
            float stuckRecoveryTimer = 0.0f;
            float stuckCooldownTimer = 0.0f;
            float fallbackTimer = 0.0f;

            float dynAggr = 0.0f;
            int   faults = 0;
            int   totalFaults = 0;
            float faultWindow = 0.0f;

            bool  sheetWroteLastTick = false;
            bool  sheetWroteThisTick = false;

            bool  leadOutValid = false;
            float leadScaleOut = 0.0f, leadBaseOut = 0.0f, leadMaxOut = 0.0f;
            bool  heightOutValid = false;
            float hSkidOut = 0.0f, hCloseOut = 0.0f, hHighOut = 0.0f;
            float clampOut = 0.0f;
            bool  clampOutValid = false;
            float budgetScaleOut = 1.0f;

            unsigned long transLastMs[10][10] = {};
            unsigned suppressedTransitions = 0;
        };

        TrackerState T;
        AttackState  K;
        AttitudeState A;
        MachineState M;
        bool gStandDown = false;
        bool gStandDownLogged = false;
        unsigned long gLastFreezeLogMs = 0;
        unsigned long gLastAiUpdateMs = 0;
        unsigned long gLastAttackDbgMs = 0;
        // Altitude diagnostics captured each tick for telemetry.
        struct AltDiag {
            float commanded = 0.0f;       // max commanded chase height offset (m)
            float actualAbove = 0.0f;     // heli.y - dest.y (m)
            float vertVel = 0.0f;         // world-up velocity (m/s)
            float dynContrib = 0.0f;      // dynamic-altitude contribution (m)
            float recovContrib = 0.0f;    // recovery-state contribution (m)
            bool  commandClamped = false; // any commanded height hit its clamp
        };
        AltDiag gAlt;

        bool InRecoveryState() {
            return M.state == AiState::DirectionRecovery
                || M.state == AiState::StuckRecovery
                || M.state == AiState::CirclingRecovery
                || M.state == AiState::AttitudeRecovery;
        }

        void ResetAll(void* ownerKey) {
            std::memset(&T, 0, sizeof(T));
            K = AttackState{};
            A.prevValid = false;
            A.unstableTime = A.stableTime = 0.0f;
            A.active = false; A.stage = 0; A.reenableTime = 1.0e6f;
            const AttitudeState savedA = A;
            const int savedTotalFaults = M.totalFaults;
            M = MachineState{};
            A = savedA;
            M.totalFaults = savedTotalFaults;
            T.ownerKey = ownerKey;
            M.state = AiState::Arrival;
        }

        void Fault() {
            if (!gCfg.EnableFallback) return;
            M.faults++;
            M.totalFaults++;
            if (!gStandDown && M.totalFaults > gCfg.PermanentStandDownFaults) {
                gStandDown = true;
                if (!gStandDownLogged) {
                    gStandDownLogged = true;
                    Log::Error("[AI] PERMANENT STAND-DOWN: %d total faults exceeded "
                               "PermanentStandDownFaults=%d. The dynamic AI layer is "
                               "disabled for this session; vanilla behavior in effect. "
                               "Send the log.", M.totalFaults, gCfg.PermanentStandDownFaults);
                }
            }
        }

        void SetState(AiState s, const char* why) {
            if (M.state == s) return;
            const int from = static_cast<int>(M.state);
            const int to = static_cast<int>(s);
            if (gCfg.EnableStateMachine && gCfg.LogStateTransitions) {
                const unsigned long now = GetTickCount();
                const unsigned long cooldown =
                    static_cast<unsigned long>(gCfg.TransitionLogCooldownSeconds * 1000.0f);
                if (now - M.transLastMs[from][to] >= cooldown) {
                    M.transLastMs[from][to] = now;
                    if (M.suppressedTransitions) {
                        Log::Info("[AI] state %s -> %s (%s) [+%u suppressed transition log(s)]",
                                  AiStateName(M.state), AiStateName(s), why,
                                  M.suppressedTransitions);
                        M.suppressedTransitions = 0;
                    } else {
                        Log::Info("[AI] state %s -> %s (%s)",
                                  AiStateName(M.state), AiStateName(s), why);
                    }
                } else {
                    M.suppressedTransitions++;
                }
            }
            M.state = s;
            M.stateTime = 0.0f;
        }

        // Shared velocity-derivative update (speed, accel, heading rate, 1s-old
        // copy) from the current T.destVel estimate.
        void DeriveFromVel(float dt, float a) {
            const float spd = Len2(T.destVel[0], T.destVel[2]);
            float accel = (spd - T.targetSpeedPrev) / dt;
            accel = Clampf(accel, -gCfg.MaxTargetAcceleration, gCfg.MaxTargetAcceleration);
            T.targetAccel = Lerp(T.targetAccel, accel, a);
            T.targetSpeedPrev = spd;
            T.targetSpeed = spd;

            if (spd > 3.0f) {
                const float dx = T.destVel[0] / spd, dz = T.destVel[2] / spd;
                if (T.tgtDirPrev[0] != 0.0f || T.tgtDirPrev[1] != 0.0f) {
                    const float dot = Clampf(dx * T.tgtDirPrev[0] + dz * T.tgtDirPrev[1], -1.0f, 1.0f);
                    T.destHeadingRateDegSec = Lerp(T.destHeadingRateDegSec,
                                              std::acos(dot) * kRad2Deg / dt, a);
                }
                T.tgtDirPrev[0] = dx; T.tgtDirPrev[1] = dz;
            } else {
                T.destHeadingRateDegSec = Lerp(T.destHeadingRateDegSec, 0.0f, a);
            }

            T.destVelOldTimer += dt;
            if (T.destVelOldTimer >= 1.0f) {
                std::memcpy(T.destVelOld, T.destVel, sizeof(T.destVel));
                T.destVelOldTimer = 0.0f;
            }
        }

        // ------------------------------------------------------------------ tracker
        void UpdateTracker(const HeliState::Snapshot& s, float dt) {
            // PRIORITY 1 source: the real local-player rigid body (verified
            // IPlayer chain). Rigid-body velocity is authoritative - player
            // teleports/resets cannot corrupt it, so no freeze logic needed.
            float ppos[3], pvel[3];
            const bool playerSrc = gCfg.UsePlayerPositionSource
                                   && HeliState::ReadPlayerKinematics(ppos, pvel);
            // Keep the latest verified player position for real attack-distance.
            T.playerPosValid = playerSrc;
            if (playerSrc) std::memcpy(T.playerPos, ppos, sizeof(T.playerPos));
            if (playerSrc != T.playerSrcActive || !T.srcLoggedOnce) {
                const unsigned long nowMs = GetTickCount();
                if (!T.srcLoggedOnce || nowMs - T.lastSrcSwitchLogMs >= 5000) {
                    T.lastSrcSwitchLogMs = nowMs;
                    T.srcLoggedOnce = true;
                    if (playerSrc)
                        Log::Info("Player tracking initialized.");
                    else
                        Log::Info("Player tracking is using an estimated position "
                                  "(direct tracking unavailable).");
                }
                T.playerSrcActive = playerSrc;
            }

            if (playerSrc) {
                const float a = Clampf(dt * 8.0f, 0.0f, 1.0f);
                T.destVel[0] = Lerp(T.destVel[0], pvel[0], a);
                T.destVel[1] = Lerp(T.destVel[1], pvel[1], a);
                T.destVel[2] = Lerp(T.destVel[2], pvel[2], a);
                if (T.validSamples < 1000) T.validSamples += 2;   // direct source
                T.freezeTimer = 0.0f;
                std::memcpy(T.destPrev, s.dest, sizeof(T.destPrev));
                T.histValid = true;
                DeriveFromVel(dt, a);
            } else
            if (T.histValid) {
                const float jump = Len2(s.dest[0] - T.destPrev[0], s.dest[2] - T.destPrev[2]);
                const float plausible = (T.targetSpeed + 60.0f) * dt + 8.0f;
                if (jump > plausible && jump > gCfg.TeleportJumpDistance) {
                    T.freezeTimer = gCfg.FreezeSeconds;
                    std::memcpy(T.destPrev, s.dest, sizeof(T.destPrev));
                    if (gCfg.LogEstimatorEvents) {
                        const unsigned long now = GetTickCount();
                        if (now - gLastFreezeLogMs >= 5000) {
                            gLastFreezeLogMs = now;
                            Log::Info("[AI] estimator frozen %.2fs (target jump %.1f m).",
                                      gCfg.FreezeSeconds, jump);
                        }
                    }
                }
            } else {
                std::memcpy(T.destPrev, s.dest, sizeof(T.destPrev));
                T.histValid = true;
                T.freezeTimer = gCfg.FreezeSeconds * 0.5f;
            }

            if (!playerSrc && T.freezeTimer > 0.0f) {
                T.freezeTimer -= dt;
                T.validSamples = T.validSamples > 4 ? T.validSamples - 4 : 0;
                std::memcpy(T.destPrev, s.dest, sizeof(T.destPrev));
            } else if (!playerSrc) {
                const float a = Clampf(dt * 6.0f, 0.0f, 1.0f);
                const float vx = (s.dest[0] - T.destPrev[0]) / dt;
                const float vy = (s.dest[1] - T.destPrev[1]) / dt;
                const float vz = (s.dest[2] - T.destPrev[2]) / dt;
                std::memcpy(T.destPrev, s.dest, sizeof(T.destPrev));

                T.destVel[0] = Lerp(T.destVel[0], vx, a);
                T.destVel[1] = Lerp(T.destVel[1], vy, a);
                T.destVel[2] = Lerp(T.destVel[2], vz, a);
                if (T.validSamples < 1000) T.validSamples++;
                DeriveFromVel(dt, a);
            }

            const float ownSpd = Len2(s.vel[0], s.vel[2]);
            if (ownSpd > 3.0f) {
                const float dx = s.vel[0] / ownSpd, dz = s.vel[2] / ownSpd;
                if (T.ownDirPrev[0] != 0.0f || T.ownDirPrev[1] != 0.0f) {
                    const float dot = Clampf(dx * T.ownDirPrev[0] + dz * T.ownDirPrev[1], -1.0f, 1.0f);
                    const float cross = T.ownDirPrev[0] * dz - T.ownDirPrev[1] * dx;
                    T.behavHeadingRateDegSec = Lerp(T.behavHeadingRateDegSec, std::acos(dot) * kRad2Deg / dt, 0.15f);
                    const float sign = (cross > 0.001f) ? 1.0f : ((cross < -0.001f) ? -1.0f : 0.0f);
                    if (sign != 0.0f && T.turnSignPrev != 0.0f && sign != T.turnSignPrev
                        && T.behavHeadingRateDegSec > 20.0f)
                        T.flipRate += 1.0f;
                    if (sign != 0.0f) T.turnSignPrev = sign;
                }
                T.ownDirPrev[0] = dx; T.ownDirPrev[1] = dz;
            }
            T.flipRate -= T.flipRate * Clampf(dt, 0.0f, 1.0f);

            const bool sampleValid = (T.freezeTimer <= 0.0f) && !s.skidActive
                                     && T.validSamples >= 5;
            if (gCfg.EnableStationaryPlayer && sampleValid) {
                if (T.targetSpeed < gCfg.StoppedSpeedThreshold) {
                    T.lowSpeedTime += dt;
                    T.lowSpeedSamples++;
                    T.fastSamples = 0;
                } else if (T.targetSpeed > gCfg.StoppedSpeedThreshold * 1.5f) {
                    if (++T.fastSamples >= gCfg.MovingClearSamples) {
                        T.lowSpeedTime = 0.0f;
                        T.lowSpeedSamples = 0;
                        T.stopped = false;
                    }
                }
                if (!T.stopped && T.lowSpeedSamples >= 5
                    && T.lowSpeedTime >= gCfg.StoppedDetectSeconds)
                    T.stopped = true;
            } else if (!gCfg.EnableStationaryPlayer) {
                T.stopped = false;
            }

            T.reversal = false;
            T.sharpTurn = false;
            if (gCfg.EnableDirectionChange && T.freezeTimer <= 0.0f
                && T.validSamples >= gCfg.MinValidSamples) {
                const float curSpd = Len2(T.destVel[0], T.destVel[2]);
                const float oldSpd = Len2(T.destVelOld[0], T.destVelOld[2]);
                if (curSpd > 5.0f && oldSpd > 5.0f) {
                    const float dot = (T.destVel[0] * T.destVelOld[0] + T.destVel[2] * T.destVelOld[2])
                                    / (curSpd * oldSpd);
                    if (dot < gCfg.ReversalDot) T.reversal = true;
                }
                T.sharpTurn = T.destHeadingRateDegSec > gCfg.SharpTurnDegPerSec;
            }
            T.braking = (T.freezeTimer <= 0.0f) && T.targetAccel < -8.0f && T.targetSpeed > 5.0f;

            if (T.destHeadingRateDegSec < 25.0f && !T.braking && !T.reversal && T.freezeTimer <= 0.0f)
                T.stablePathTime += dt;
            else
                T.stablePathTime = 0.0f;

            {
                const float toX = s.dest[0] - s.pos[0], toZ = s.dest[2] - s.pos[2];
                const float d = Len2(toX, toZ);
                float progress = 0.0f;
                if (d > 1.0f) progress = (s.vel[0] * toX + s.vel[2] * toZ) / d;
                if (gCfg.EnableStuckRecovery && s.driveSpeed > 15.0f && d > 25.0f
                    && progress < gCfg.StuckMinProgressSpeed && !s.skidActive)
                    T.noProgressTime += dt;
                else
                    T.noProgressTime = 0.0f;

                // Circling must be a real PHYSICAL rotation, confidence-gated.
                // A.angSpeed (rad/s) comes from orientation-vector deltas and
                // is only meaningful once the up-vector is calibrated
                // (A.upOrder != 0 && A.valid). The behavioral heading rate is
                // NOT used here - it spiked to 318 deg/s in testing while the
                // body barely rotated (angular=0.03 rad/s). AntiCircling thus
                // stays effectively gated off until a genuine spin occurs.
                const float physThreshRad = gCfg.CirclingTurnDegPerSec / kRad2Deg;
                const bool physConfident = A.valid && A.upOrder != 0;
                const bool circling =
                    physConfident && A.angSpeed > physThreshRad && d < 80.0f;
                if (gCfg.EnableAntiCircling && circling && !s.skidActive)
                    T.circlingTime += dt;
                else
                    T.circlingTime = 0.0f;
            }
        }

        // ------------------------------------------------------------------ attitude
        void UpdateAttitude(const HeliState::Snapshot& s, float dt) {
            A.valid = false;
            if (!gCfg.EnableAttitudeStability) return;
            if (!s.fwdRightValid) {
                A.prevValid = false;
                if (!A.unavailableLogged) {
                    A.unavailableLogged = true;
                    Log::Warn("[Attitude] orientation vectors unavailable/failed sanity "
                              "checks - attitude stability system inactive. Send this log.");
                }
                return;
            }

            float up[3];
            Cross3(s.fwd, s.right, up);

            if (A.upOrder == 0) {
                const float ownSpd = Len2(s.vel[0], s.vel[2]);
                if (ownSpd > 8.0f && std::fabs(up[1]) > 0.5f) {
                    A.calibSum += up[1];
                    if (++A.calibCount >= 30) {
                        A.upOrder = (A.calibSum > 0.0f) ? 1 : 2;
                        if (gCfg.LogAttitudeEvents)
                            Log::Info("[Attitude] up-vector calibrated: up = %scross(fwd,right).",
                                      A.upOrder == 1 ? "" : "-");
                    }
                }
                return;
            }
            if (A.upOrder == 2) { up[0] = -up[0]; up[1] = -up[1]; up[2] = -up[2]; }
            const float ulen = std::sqrt(Dot3(up, up));
            if (ulen < 0.5f) { A.prevValid = false; return; }
            up[0] /= ulen; up[1] /= ulen; up[2] /= ulen;

            A.valid = true;
            A.upDot = Clampf(up[1], -1.0f, 1.0f);
            A.rollDeg  = std::asin(Clampf(s.right[1], -1.0f, 1.0f)) * kRad2Deg;
            A.pitchDeg = std::asin(Clampf(s.fwd[1],   -1.0f, 1.0f)) * kRad2Deg;

            if (A.prevValid && dt > 0.0f) {
                const float df = std::acos(Clampf(Dot3(s.fwd, A.prevFwd), -1.0f, 1.0f)) / dt;
                const float du = std::acos(Clampf(Dot3(up, A.prevUp), -1.0f, 1.0f)) / dt;
                const float ang = df > du ? df : du;
                A.angSpeed = Lerp(A.angSpeed, ang, 0.3f);
            }
            std::memcpy(A.prevFwd, s.fwd, sizeof(A.prevFwd));
            std::memcpy(A.prevUp, up, sizeof(A.prevUp));
            A.prevValid = true;

            const bool unstable =
                std::fabs(A.rollDeg)  > gCfg.MaximumSafeRollDegrees
                || std::fabs(A.pitchDeg) > gCfg.MaximumSafePitchDegrees
                || A.angSpeed > gCfg.MaximumAngularSpeed;

            const float tilt = std::fabs(A.rollDeg) > std::fabs(A.pitchDeg)
                             ? std::fabs(A.rollDeg) : std::fabs(A.pitchDeg);
            if (unstable) {
                A.unstableTime += dt;
                A.stableTime = 0.0f;
            } else if (tilt < gCfg.RecoveryReleaseDegrees
                       && A.angSpeed < gCfg.MaximumAngularSpeed * 0.5f) {
                A.stableTime += dt;
                A.unstableTime = A.unstableTime > dt * 2 ? A.unstableTime - dt * 2 : 0.0f;
            }

            if (!A.active && A.unstableTime >= gCfg.UnstableDetectSeconds) {
                A.active = true;
                A.stage = 1;
                A.recoveryTime = 0.0f;
                if (gCfg.LogAttitudeEvents)
                    Log::Warn("[Attitude] LOSS OF UPRIGHT CONTROL: roll=%.0f pitch=%.0f "
                              "angular=%.2f rad/s upDot=%.2f - entering recovery stage 1.",
                              A.rollDeg, A.pitchDeg, A.angSpeed, A.upDot);
            }
            if (A.active) {
                A.recoveryTime += dt;
                const int newStage = A.recoveryTime > gCfg.Stage3Seconds ? 3
                                   : (A.recoveryTime > gCfg.Stage2Seconds ? 2 : 1);
                if (newStage != A.stage) {
                    A.stage = newStage;
                    if (gCfg.LogAttitudeEvents)
                        Log::Warn("[Attitude] still unstable after %.1fs - recovery stage %d "
                                  "(roll=%.0f pitch=%.0f angular=%.2f).",
                                  A.recoveryTime, A.stage, A.rollDeg, A.pitchDeg, A.angSpeed);
                }
                if (A.stableTime >= gCfg.RecoveryStableSeconds) {
                    if (gCfg.LogAttitudeEvents)
                        Log::Info("[Attitude] upright control recovered after %.1fs "
                                  "(roll=%.0f pitch=%.0f). Speed assist re-enabling over %.1fs.",
                                  A.recoveryTime, A.rollDeg, A.pitchDeg,
                                  gCfg.SpeedAssistReenableSeconds);
                    A.active = false;
                    A.stage = 0;
                    A.unstableTime = 0.0f;
                    A.reenableTime = 0.0f;
                }
            } else if (A.reenableTime < gCfg.SpeedAssistReenableSeconds) {
                A.reenableTime += dt;
            }
        }

        // Horizontal helicopter-to-player distance from verified positions.
        // Returns false when the player position is unavailable this tick.
        bool HeliPlayerDistance(const HeliState::Snapshot& s, float* out) {
            if (!T.playerPosValid) return false;
            *out = Len2(s.pos[0] - T.playerPos[0], s.pos[2] - T.playerPos[2]);
            return true;
        }

        // Finalize an accepted attack: classify from the minimum distance and
        // update counters/timers. Distance that was never valid is Unknown -
        // never auto-classified as an abort.
        void EndAttack(const char* endReason) {
            K.active = false;
            K.posEvidence = 0.0f;
            K.endReason = endReason;

            if (!K.minDistValid) {
                K.classification = "unclassified";
                K.unclassified++;
            } else if (K.minDist <= kEngageDistance) {
                K.classification = "engagement";
                K.engagements++;
                if (gCfg.EnableDynamicAggression && gCfg.DynAggrResetOnEngagement)
                    M.dynAggr = gCfg.DynAggrMin;
            } else {
                K.classification = "abort";
                K.aborts++;
                M.backoffTimer = gCfg.FailedAttackBackoffSeconds;
                if (gCfg.EnableDynamicAggression)
                    M.dynAggr = Clampf(M.dynAggr + gCfg.DynAggrFailedAttackIncrease,
                                       gCfg.DynAggrMin, gCfg.DynAggrMax);
            }
            if (gCfg.ReattackDelaySeconds > 0.0f)
                M.reattackTimer = gCfg.ReattackDelaySeconds;

            if (gCfg.LogAttackEvents) {
                char distBuf[40];
                if (K.minDistValid)
                    std::snprintf(distBuf, sizeof(distBuf), "min dist %.1f m", K.minDist);
                else
                    std::snprintf(distBuf, sizeof(distBuf), "min dist unavailable");
                Log::Info("Helicopter attack #%d ended after %.1fs (%s): %s [%s].",
                          K.id, K.lifeTime, distBuf, K.classification, K.endReason);
            }
        }

        // ------------------------------------------------------------------ attacks
        // Start detection uses a pulse-tolerant EVIDENCE latch (the raw skid
        // flag pulses at tick frequency and never stays continuously true).
        // COMPLETION is driven by the verified helicopter-to-player distance,
        // NOT by the raw flag dropping - so attacks last a realistic time and
        // are classified from how close the helicopter actually got.
        void UpdateAttackObservation(const HeliState::Snapshot& s, float dt) {
            const bool rawSignal = s.skidActive;
            // Mask self-write pulses (the sheet override can set the flag for a
            // single tick) only when NOT already in an attack.
            const bool suspect = M.sheetWroteLastTick && rawSignal && !K.active;
            const bool raw = rawSignal && !suspect;
            if (suspect) K.suppressedPulses++;
            if (rawSignal && !K.rawPrev) K.rawPulses++;   // raw rising edges
            K.rawPrev = rawSignal;

            const char* reject = "";

            if (!K.active) {
                if (raw) {
                    K.posEvidence += dt;
                    K.windowTimer = kEvidenceWindowSeconds;
                } else {
                    // Brief gaps keep evidence; only decay after the window.
                    K.windowTimer -= dt;
                    if (K.windowTimer <= 0.0f) {
                        K.windowTimer = 0.0f;
                        K.posEvidence -= dt;
                        if (K.posEvidence < 0.0f) K.posEvidence = 0.0f;
                    }
                }

                const bool timersBlock = (M.reattackTimer > 0.0f || M.backoffTimer > 0.0f);
                const bool enough = K.posEvidence >= gCfg.AttackStartDebounceSeconds;
                const bool strong = K.posEvidence >= 1.0f;
                if (!enough) reject = "insufficient-evidence";
                else if (timersBlock && !strong) reject = "reattack/backoff-gate";

                if (enough && (!timersBlock || strong)) {
                    K.active = true;
                    K.id++;
                    K.acceptedEdges++;
                    K.lifeTime = 0.0f;
                    K.minDist = 0.0f;
                    K.minDistValid = false;
                    K.lastDist = 0.0f;
                    K.lastDistValid = false;
                    K.passedTarget = false;
                    K.risingTime = 0.0f;
                    K.timeSinceRawPulse = 0.0f;
                    K.windowTimer = kEvidenceWindowSeconds;
                    K.endReason = "";
                    K.classification = "";
                    K.attacksObserved++;
                    if (gCfg.LogAttackEvents)
                        Log::Info("Helicopter attack #%d started.", K.id);
                    K.posEvidence = 0.0f;
                }
            } else {
                K.lifeTime += dt;
                if (raw) K.timeSinceRawPulse = 0.0f;
                else     K.timeSinceRawPulse += dt;

                // Track the real helicopter-to-player distance every tick.
                float d = 0.0f;
                const bool distOk = HeliPlayerDistance(s, &d);
                if (distOk) {
                    K.lastDist = d;
                    K.lastDistValid = true;
                    if (!K.minDistValid || d < K.minDist) { K.minDist = d; K.minDistValid = true; }
                    // Passed-target: distance climbed clearly above the minimum
                    // after having been reasonably close.
                    if (K.minDistValid && K.minDist < 40.0f && d > K.minDist + 5.0f) {
                        K.risingTime += dt;
                        if (K.risingTime >= 0.3f) K.passedTarget = true;
                    } else {
                        K.risingTime = 0.0f;
                    }
                }

                // ---- completion, in priority order ----
                const char* endReason = nullptr;
                if (gCfg.MaxStrikeSeconds > 0.0f && K.lifeTime > gCfg.MaxStrikeSeconds)
                    endReason = "MaxStrikeTimeout";
                else if (T.reversal || M.dirRecoveryTimer > 0.0f)
                    endReason = "ReversalAbort";
                else if (A.active || M.state == AiState::AttitudeRecovery)
                    endReason = "StateRecovery";
                else if (K.lifeTime >= kMinAttackSeconds) {
                    if (K.passedTarget)
                        endReason = (K.minDistValid && K.minDist <= kEngageDistance)
                                  ? "StrikeCompleted" : "PassedTarget";
                    else if (K.minDistValid && K.minDist <= kEngageDistance
                             && K.lastDistValid && K.lastDist > K.minDist + 2.0f)
                        endReason = "MinimumDistanceReached";
                    else if (K.timeSinceRawPulse >= kRawGapEndSeconds)
                        endReason = K.lastDistValid ? "LostTarget" : "InvalidTracking";
                }

                if (endReason) EndAttack(endReason);
                else reject = "attack-active";
            }

            // Rate-limited lifecycle diagnostics (only while there is activity,
            // and only when logging is enabled).
            if (gCfg.LogAttackEvents && (rawSignal || K.posEvidence > 0.0f || K.active)) {
                const unsigned long now = GetTickCount();
                if (now - gLastAttackDbgMs >= 2000) {
                    gLastAttackDbgMs = now;
                    char distBuf[24];
                    if (K.active && K.lastDistValid)
                        std::snprintf(distBuf, sizeof(distBuf), "%.1f", K.lastDist);
                    else
                        std::snprintf(distBuf, sizeof(distBuf), "n/a");
                    Log::Info("[Diag:Attack] raw=%d latched=%d elapsed=%.1f evidence=%.2f "
                              "sinceRawPulse=%.2f dist=%s minDist=%.1f distValid=%d passed=%d "
                              "starts=%d attacks=%d engaged=%d aborts=%d unknown=%d state=%s",
                              rawSignal ? 1 : 0, K.active ? 1 : 0, K.lifeTime, K.posEvidence,
                              K.timeSinceRawPulse, distBuf,
                              K.minDistValid ? K.minDist : -1.0f,
                              (K.active && K.lastDistValid) ? 1 : 0, K.passedTarget ? 1 : 0,
                              K.acceptedEdges, K.attacksObserved, K.engagements, K.aborts,
                              K.unclassified, reject[0] ? reject : "none");
                }
            }
        }

        // ------------------------------------------------------------------ machine
        void UpdateStateMachine(float dt) {
            M.stateTime += dt;
            M.aliveTime += dt;
            M.reattackTimer      = M.reattackTimer      > 0 ? M.reattackTimer - dt      : 0;
            M.backoffTimer       = M.backoffTimer       > 0 ? M.backoffTimer - dt       : 0;
            M.dirRecoveryTimer   = M.dirRecoveryTimer   > 0 ? M.dirRecoveryTimer - dt   : 0;
            M.dirCooldownTimer   = M.dirCooldownTimer   > 0 ? M.dirCooldownTimer - dt   : 0;
            M.circRecoveryTimer  = M.circRecoveryTimer  > 0 ? M.circRecoveryTimer - dt  : 0;
            M.circCooldownTimer  = M.circCooldownTimer  > 0 ? M.circCooldownTimer - dt  : 0;
            M.stuckRecoveryTimer = M.stuckRecoveryTimer > 0 ? M.stuckRecoveryTimer - dt : 0;
            M.stuckCooldownTimer = M.stuckCooldownTimer > 0 ? M.stuckCooldownTimer - dt : 0;
            M.fallbackTimer      = M.fallbackTimer      > 0 ? M.fallbackTimer - dt      : 0;

            M.faultWindow += dt;
            if (M.faultWindow >= 60.0f) { M.faultWindow = 0.0f; M.faults = 0; }
            if (gCfg.EnableFallback && M.faults > gCfg.FallbackMaxFaultsPerMinute
                && M.state != AiState::Fallback) {
                M.fallbackTimer = gCfg.FallbackSeconds;
                SetState(AiState::Fallback, "fault threshold exceeded");
                return;
            }

            if (gCfg.EnableStateMachine && M.state != AiState::Chasing
                && M.state != AiState::Attacking && M.state != AiState::Fallback
                && M.stateTime > gCfg.StateTimeoutSeconds) {
                M.fallbackTimer = gCfg.FallbackSeconds;
                SetState(AiState::Fallback, "state watchdog timeout");
                return;
            }

            if ((T.reversal || T.sharpTurn) && gCfg.EnableDirectionChange
                && M.dirRecoveryTimer <= 0 && M.dirCooldownTimer <= 0) {
                M.dirRecoveryTimer = gCfg.DirectionRecoverySeconds;
                M.dirCooldownTimer = gCfg.DirectionRecoverySeconds + gCfg.DirectionCooldownSeconds;
            }
            if (T.circlingTime >= gCfg.CirclingDetectSeconds && gCfg.EnableAntiCircling
                && M.circCooldownTimer <= 0) {
                if (M.circRecoveryTimer <= 0) {
                    M.circRecoveryTimer = gCfg.CirclingRecoverySeconds;
                    M.circCooldownTimer = gCfg.CirclingRecoverySeconds + gCfg.CirclingCooldownSeconds;
                    Log::Info("[AI] circling/oscillation detected (turn %.0f deg/s, flips %.1f/s).",
                              T.behavHeadingRateDegSec, T.flipRate);
                }
                T.circlingTime = 0.0f;
            }
            if (T.noProgressTime >= gCfg.StuckNoProgressSeconds && gCfg.EnableStuckRecovery
                && M.stuckCooldownTimer <= 0) {
                M.stuckRecoveryTimer = gCfg.StuckRecoverySeconds;
                M.stuckCooldownTimer = gCfg.StuckRecoveryCooldownSeconds;
                T.noProgressTime = 0.0f;
                Log::Info("[AI] stuck detected (no progress toward target).");
            }

            if (M.state == AiState::Fallback) {
                if (M.fallbackTimer > 0) return;
                SetState(AiState::Chasing, "fallback expired");
            }
            if (A.active)                       { SetState(AiState::AttitudeRecovery, "upright control lost"); return; }
            if (M.stuckRecoveryTimer > 0)       { SetState(AiState::StuckRecovery, "no progress"); return; }
            if (M.circRecoveryTimer > 0)        { SetState(AiState::CirclingRecovery, "rotation"); return; }
            if (M.dirRecoveryTimer > 0)         { SetState(AiState::DirectionRecovery, "target direction change"); return; }
            if (K.active)                       { SetState(AiState::Attacking, "skid attack lifecycle"); return; }
            if (M.reattackTimer > 0)            { SetState(AiState::PostAttackRecovery, "reattack delay"); return; }
            if (T.stopped)                      { SetState(AiState::StoppedPlayerHold, "player stopped"); return; }
            if (gCfg.EnableAggression && M.aliveTime < gCfg.ArrivalGraceSeconds)
                                                { SetState(AiState::Arrival, "grace"); return; }
            SetState(AiState::Chasing, "default");
        }

        // ------------------------------------------------------------------ slewed writers
        void WriteLeadSlewLimited(float scale, float base, float maxLead, float dt) {
            base = Clampf(base, gCfg.LeadMin, 100.0f);
            if (!M.leadOutValid) {
                M.leadScaleOut = scale; M.leadBaseOut = base; M.leadMaxOut = maxLead;
                M.leadOutValid = true;
            } else {
                const float dScale = gCfg.LeadSlewScaleRate * dt;
                const float dBase  = gCfg.LeadSlewBaseRate * dt;
                const float dMax   = gCfg.LeadSlewBaseRate * 1.5f * dt;
                if (std::fabs(scale - M.leadScaleOut) > gCfg.LeadDeadZone * 0.02f)
                    M.leadScaleOut += Clampf(scale - M.leadScaleOut, -dScale, dScale);
                if (std::fabs(base - M.leadBaseOut) > gCfg.LeadDeadZone)
                    M.leadBaseOut += Clampf(base - M.leadBaseOut, -dBase, dBase);
                if (std::fabs(maxLead - M.leadMaxOut) > gCfg.LeadDeadZone)
                    M.leadMaxOut += Clampf(maxLead - M.leadMaxOut, -dMax, dMax);
            }
            gLive.LeadSpeedScale = Clampf(M.leadScaleOut, 0.0f, 2.0f);
            gLive.LeadBase       = Clampf(M.leadBaseOut, 0.0f, 100.0f);
            gLive.LeadMax        = Clampf(M.leadMaxOut, 5.0f, 150.0f);
        }

        void WriteHeightsSlewLimited(float hSkid, float hClose, float hHigh, float dt) {
            hSkid  = Clampf(hSkid,  gCfg.MinCommandedHeight, gCfg.MaxCommandedHeight);
            hClose = Clampf(hClose, gCfg.MinCommandedHeight, gCfg.MaxCommandedHeight);
            hHigh  = Clampf(hHigh,  gCfg.MinCommandedHeight, gCfg.MaxCommandedHeight);
            // heightCommandClamped diagnostic: did any input hit the window?
            gAlt.commandClamped =
                (hClose >= gCfg.MaxCommandedHeight - 0.01f) ||
                (hHigh  >= gCfg.MaxCommandedHeight - 0.01f) ||
                (hSkid  <= gCfg.MinCommandedHeight + 0.01f);
            if (!M.heightOutValid) {
                M.hSkidOut = hSkid; M.hCloseOut = hClose; M.hHighOut = hHigh;
                M.heightOutValid = true;
            } else {
                const float dH = gCfg.HeightSlewRate * dt;
                M.hSkidOut  += Clampf(hSkid - M.hSkidOut, -dH, dH);
                M.hCloseOut += Clampf(hClose - M.hCloseOut, -dH, dH);
                M.hHighOut  += Clampf(hHigh - M.hHighOut, -dH, dH);
            }
            gLive.ChaseHeightSkid  = Clampf(M.hSkidOut, -5.0f, 30.0f);
            gLive.ChaseHeightClose = Clampf(M.hCloseOut, -5.0f, 30.0f);
            gLive.ChaseHeightHigh  = Clampf(M.hHighOut, -5.0f, 50.0f);
            // Commanded = the largest offset we ask the game to hold.
            gAlt.commanded = gLive.ChaseHeightHigh > gLive.ChaseHeightClose
                           ? gLive.ChaseHeightHigh : gLive.ChaseHeightClose;
        }

        void WriteClampSlewLimited(float clamp, float dt) {
            if (!M.clampOutValid) { M.clampOut = clamp; M.clampOutValid = true; }
            else {
                const float alpha = Clampf(dt / (gCfg.SteeringSlewSeconds > dt
                                                 ? gCfg.SteeringSlewSeconds : dt), 0.0f, 1.0f);
                M.clampOut = Lerp(M.clampOut, clamp, alpha);
            }
            const float c = Clampf(M.clampOut, 0.4f, 5.0f);
            gLive.TurnClampPos = c;
            gLive.TurnClampNeg = -c;
        }

        void WriteBudgetScale(const HeliState::Snapshot& s, float dt) {
            if (!gCfg.EnableAccelPatches) return;
            float target = 1.0f;
            if (M.state == AiState::Attacking) target = gCfg.AttackBudgetScale;
            else if (InRecoveryState())        target = gCfg.RecoveryBudgetScale;
            (void)s;
            const float rate = (gCfg.BudgetSlewRate / (gCfg.AccelBudgetMax > 1.0f
                                                       ? gCfg.AccelBudgetMax : 1.0f)) * dt;
            M.budgetScaleOut += Clampf(target - M.budgetScaleOut, -rate, rate);
            const float maxV = gCfg.AccelBudgetMax * M.budgetScaleOut;
            const float minV = gCfg.AccelBudgetMin * M.budgetScaleOut;
            static float lastMax = -1.0f;
            if (std::fabs(maxV - lastMax) > 0.25f) {
                lastMax = maxV;
                Memory::WriteBytes(Addr::kMaxChopperAccel, &maxV, 4);
                Memory::WriteBytes(Addr::kMinChopperAccel, &minV, 4);
            }
        }

        // ------------------------------------------------------------------ tuning
        void ApplyDynamicTuning(const HeliState::Snapshot& s, float dt) {
            if (M.state == AiState::Fallback) {
                gLive.LeadSpeedScale        = Addr::Vanilla::LeadSpeedScale;
                gLive.LeadBase              = Addr::Vanilla::LeadBase;
                gLive.LeadMax               = Addr::Vanilla::LeadMax;
                gLive.LeadSkidMultiplier    = Addr::Vanilla::LeadSkidMultiplier;
                gLive.ChaseHeightSkid       = Addr::Vanilla::ChaseHeightSkid;
                gLive.ChaseHeightClose      = Addr::Vanilla::ChaseHeightClose;
                gLive.ChaseHeightHigh       = Addr::Vanilla::ChaseHeightHigh;
                gLive.SkidCooldownThreshold = Addr::Vanilla::SkidCooldown;
                gLive.SkidEntryMaxDistance  = Addr::Vanilla::EntryMaxDistance;
                gLive.SkidEntryMinDistance  = Addr::Vanilla::EntryMinDistance;
                gLive.SkidEntryAlignmentDot = Addr::Vanilla::EntryAlignmentDot;
                gLive.SkidEntryMaxHeightDelta = Addr::Vanilla::EntryMaxHeightDelta;
                gLive.AbortAheadSq          = Addr::Vanilla::AbortAheadSq;
                gLive.AbortBehindSq         = Addr::Vanilla::AbortBehindSq;
                gLive.TurnResponseScale     = Addr::Vanilla::TurnResponseScale;
                gLive.TurnClampPos          = Addr::Vanilla::TurnClampPos;
                gLive.TurnClampNeg          = Addr::Vanilla::TurnClampNeg;
                gLive.SmoothingOldWeight    = Addr::Vanilla::SmoothingOldWeight;
                gLive.SmoothingFinalScale   = Addr::Vanilla::SmoothingFinalScale;
                M.leadOutValid = false;
                M.heightOutValid = false;
                M.clampOutValid = false;
                return;
            }

            const bool attitude = (M.state == AiState::AttitudeRecovery);

            // ---- aggression ------------------------------------------------------
            float aggr = 1.0f;
            if (gCfg.EnableDynamicAggression) aggr = M.dynAggr;
            const bool aggrOn = gCfg.EnableAggression && !attitude
                                && M.aliveTime >= gCfg.ArrivalGraceSeconds
                                && M.backoffTimer <= 0.0f;

            float attackFreq = 1.0f, willingness = 1.0f, alignTol = 0.0f;
            if (aggrOn) {
                attackFreq  = Lerp(1.0f, gCfg.AttackFrequency,   aggr);
                willingness = Lerp(1.0f, gCfg.AttackWillingness, aggr);
                alignTol    = Lerp(0.0f, gCfg.AlignmentTolerance, aggr);
            }

            // ---- skid entry gates ----------------------------------------------------
            float cooldown = gCfg.SkidCooldownThreshold;
            float entryMin = gCfg.SkidEntryMinDistance;
            float entryMax = gCfg.SkidEntryMaxDistance;
            float entryDot = gCfg.SkidEntryAlignmentDot;
            if (aggrOn) {
                cooldown = Lerp(-10.0f, -0.5f, Clampf(attackFreq * 0.5f, 0.0f, 1.0f));
                entryMax = Clampf(gCfg.SkidEntryMaxDistance * willingness, 5.0f, 150.0f);
                entryDot = Clampf(gCfg.SkidEntryAlignmentDot - alignTol, -1.0f, 0.99f);
            }
            if (gCfg.AttackOnlyWhenAligned && entryDot < gCfg.AlignedModeDot)
                entryDot = gCfg.AlignedModeDot;

            const bool suppressAttacks =
                gCfg.FollowOnly
                || attitude
                || (T.stopped && gCfg.SuppressAttacksWhenStopped)
                || M.reattackTimer > 0.0f
                || M.backoffTimer > 0.0f;
            if (suppressAttacks) {
                entryMin = 25.0f;
                entryMax = 5.0f;
                cooldown = -10.0f;
            }
            gLive.SkidCooldownThreshold   = cooldown;
            gLive.SkidEntryMinDistance    = entryMin;
            gLive.SkidEntryMaxDistance    = entryMax;
            gLive.SkidEntryAlignmentDot   = entryDot;
            gLive.SkidEntryMaxHeightDelta = gCfg.SkidEntryMaxHeightDelta;

            // ---- abort squeeze ----------------------------------------------------------
            // When a reversal, attitude-recovery, or max-strike condition holds
            // during an attack, tighten the game's own break-off distances so
            // the helicopter disengages cleanly. The logical attack end and its
            // reason are recorded separately by the attack tracker.
            float abortAhead = gCfg.SkidAbortDistanceAheadSq;
            float abortBehind = gCfg.SkidAbortDistanceBehindSq;
            const bool squeeze = K.active &&
                ((gCfg.AbortAttackOnReversal && (T.reversal || M.dirRecoveryTimer > 0.0f))
                 || (gCfg.MaxStrikeSeconds > 0.0f && K.lifeTime > gCfg.MaxStrikeSeconds)
                 || attitude);
            if (squeeze) {
                abortAhead = 100.0f;
                abortBehind = 25.0f;
            }
            gLive.AbortAheadSq  = abortAhead;
            gLive.AbortBehindSq = abortBehind;

            // ---- lead -----------------------------------------------------------------------
            float leadScale = gCfg.LeadSpeedScale;
            float leadBase  = gCfg.LeadBase;
            float leadMax   = gCfg.LeadMax;
            if (gCfg.EnablePrediction) {
                leadScale *= gCfg.PredictionLeadScale;
                float cut = 0.0f;
                if (gCfg.EnableDirectionChange && M.dirRecoveryTimer > 0.0f)
                    cut = gCfg.ReversalLeadReduction;
                else {
                    const float turnCut = gCfg.TurnLeadReduction
                                        * Clampf(T.destHeadingRateDegSec / 120.0f, 0.0f, 1.0f);
                    const float brakeCut = T.braking ? gCfg.BrakeLeadReduction : 0.0f;
                    cut = turnCut > brakeCut ? turnCut : brakeCut;
                }
                if (T.targetSpeed > gCfg.HighSpeedThreshold
                    && T.stablePathTime > gCfg.StablePathSeconds)
                    leadScale *= (1.0f + gCfg.HighSpeedLeadBoost);
                leadScale *= (1.0f - Clampf(cut, 0.0f, 0.95f));
                leadBase  *= (1.0f - Clampf(cut, 0.0f, 0.6f));
            }
            if (M.state == AiState::Arrival) {
                leadScale *= gCfg.ArrivalLeadScale;
                leadBase  *= gCfg.ArrivalLeadScale;
            }
            if (T.stopped) leadBase *= gCfg.StoppedLeadScale;
            if (M.state == AiState::StuckRecovery) {
                leadScale *= gCfg.StuckRecoveryLeadScale;
                leadBase  *= gCfg.StuckRecoveryLeadScale;
            }
            if (M.state == AiState::CirclingRecovery || attitude) {
                leadScale *= 0.5f;
                leadBase  *= 0.5f;
            }
            WriteLeadSlewLimited(leadScale, leadBase, leadMax, dt);
            gLive.LeadSkidMultiplier = gCfg.LeadSkidMultiplier;

            // ---- altitude ---------------------------------------------------------------------
            // These are the mod's own small height OFFSETS, clamped to the
            // Min/MaxCommandedHeight window. Large real separation over complex
            // terrain comes from the game's own navigation raising the flight
            // point, which the mod does not fully override (see the altitude
            // note in KNOWN_LIMITATIONS).
            float hSkid = gCfg.ChaseHeightSkid;
            float hClose = gCfg.ChaseHeightClose;
            float hHigh = gCfg.ChaseHeightHigh;
            const float ownSpd = Len2(s.vel[0], s.vel[2]);
            float dynContrib = 0.0f, recovContrib = 0.0f;
            if (gCfg.EnableDynamicAltitude) {
                if (ownSpd > gCfg.DynAltHighSpeedThreshold) {
                    hClose += gCfg.DynAltHighSpeedBoost;
                    hHigh  += gCfg.DynAltHighSpeedBoost;
                    dynContrib += gCfg.DynAltHighSpeedBoost;
                }
                if (gCfg.TurnHeightBoost > 0.0f && T.behavHeadingRateDegSec > 60.0f) {
                    const float t = Clampf((T.behavHeadingRateDegSec - 60.0f) / 60.0f, 0.0f, 1.0f);
                    hClose += gCfg.TurnHeightBoost * t;
                    hHigh  += gCfg.TurnHeightBoost * t;
                    dynContrib += gCfg.TurnHeightBoost * t;
                }
            }
            if (M.state == AiState::Arrival && gCfg.ArrivalHeightBoost > 0.0f) {
                hClose += gCfg.ArrivalHeightBoost;
                hHigh  += gCfg.ArrivalHeightBoost;
                dynContrib += gCfg.ArrivalHeightBoost;
            }
            if (M.state == AiState::StuckRecovery) {
                hClose += gCfg.StuckRecoveryExtraHeight;
                hHigh  += gCfg.StuckRecoveryExtraHeight;
                recovContrib += gCfg.StuckRecoveryExtraHeight;
            }
            if (attitude) {
                hSkid += gCfg.RecoveryHeightBoost * 0.6f;
                hClose += gCfg.RecoveryHeightBoost;
                hHigh  += gCfg.RecoveryHeightBoost;
                recovContrib += gCfg.RecoveryHeightBoost;
            }
            gAlt.dynContrib = dynContrib;
            gAlt.recovContrib = recovContrib;
            gAlt.actualAbove = s.pos[1] - s.dest[1];
            gAlt.vertVel = s.vel[1];
            WriteHeightsSlewLimited(hSkid, hClose, hHigh, dt);

            // ---- steering + smoothing ------------------------------------------------------------------
            if (M.state == AiState::CirclingRecovery || attitude) {
                gLive.TurnResponseScale = Addr::Vanilla::TurnResponseScale;
                float clamp = Addr::Vanilla::TurnClampPos;
                if (gCfg.TurnClamp < clamp) clamp = gCfg.TurnClamp;
                if (attitude && A.stage >= 2) clamp = 0.8f;
                WriteClampSlewLimited(clamp, dt);
                if (attitude) {
                    gLive.SmoothingOldWeight  = Addr::Vanilla::SmoothingOldWeight;
                    gLive.SmoothingFinalScale = Addr::Vanilla::SmoothingFinalScale;
                }
            } else {
                gLive.TurnResponseScale = gCfg.TurnResponseScale;
                float clamp = gCfg.TurnClamp;
                if (ownSpd > gCfg.HighSpeedClampThreshold) clamp *= gCfg.HighSpeedTurnClampScale;
                else if (ownSpd < gCfg.LowSpeedClampThreshold) clamp *= gCfg.LowSpeedTurnClampScale;
                if (M.state == AiState::Attacking) clamp *= gCfg.AttackTurnClampScale;
                WriteClampSlewLimited(clamp, dt);
                gLive.SmoothingOldWeight  = gCfg.SmoothingOldWeight;
                gLive.SmoothingFinalScale = gCfg.SmoothingFinalScale;
            }

            WriteBudgetScale(s, dt);
        }

        // ------------------------------------------------------------------ drive speed
        void ApplyDriveSpeedShaping(const HeliState::Snapshot& s) {
            if (M.state == AiState::Fallback || s.skidActive) return;
            if (s.driveSpeed < 0.0f) return;

            float ds = s.driveSpeed;
            const float orig = ds;

            if (M.state == AiState::AttitudeRecovery) {
                const float cut = (A.stage >= 3) ? gCfg.Stage3SpeedCut
                                : (A.stage == 2 ? gCfg.Stage2SpeedCut : gCfg.Stage1SpeedCut);
                ds *= cut;
            } else {
                const float assist = SpeedAssistScale(nullptr);
                if (M.state == AiState::Arrival)           ds *= gCfg.ArrivalSpeedScale;
                else if (M.state == AiState::StoppedPlayerHold) ds *= gCfg.StoppedSpeedScale;
                else if (M.state == AiState::DirectionRecovery) ds *= gCfg.ReversalSpeedScale;
                else if (M.state == AiState::StuckRecovery
                         || M.state == AiState::CirclingRecovery) ds *= gCfg.RecoverySpeedScale;
                if (gCfg.EnableAggression && gCfg.CatchUpUrgency > 0.0f && assist > 0.0f
                    && M.aliveTime >= gCfg.ArrivalGraceSeconds) {
                    const float dist = Len2(s.dest[0] - s.pos[0], s.dest[2] - s.pos[2]);
                    if (dist > gCfg.CatchUpStartDistance) {
                        const float t = Clampf((dist - gCfg.CatchUpStartDistance)
                                               / gCfg.CatchUpStartDistance, 0.0f, 1.0f);
                        ds *= 1.0f + gCfg.CatchUpUrgency * gCfg.CatchUpMaxSpeedBoost * t * assist;
                    }
                }
                if (gCfg.MinChaseSpeed > 0.0f && M.state == AiState::Chasing
                    && assist >= 1.0f && T.validSamples >= gCfg.MinValidSamples
                    && T.freezeTimer <= 0.0f
                    && T.targetSpeed > 5.0f && ds < gCfg.MinChaseSpeed)
                    ds = gCfg.MinChaseSpeed;
                if (gCfg.MaxChaseSpeed > 0.0f && ds > gCfg.MaxChaseSpeed)
                    ds = gCfg.MaxChaseSpeed;
            }

            if (ds != orig)
                HeliState::WriteDriveSpeed(s.heli, ds);
        }

        // ------------------------------------------------------------------ attitude damping
        void ApplyAttitudeDamping(const HeliState::Snapshot& s, float dt) {
            if (M.state != AiState::AttitudeRecovery || A.stage < 2) return;
            // Per-frame retention factor: converted so the same proportion of
            // speed is bled off per second at any frame rate.
            const float k = FrameTime::Damping(
                (A.stage >= 3) ? gCfg.Stage3Damping : gCfg.Stage2Damping, dt);
            HeliState::WriteHorizontalVelocity(s, s.vel[0] * k, s.vel[2] * k);
        }

        // ------------------------------------------------------------------ heli sheet
        // Optional safe override: when the helicopter parks well above the
        // player at close range (a quirk of the game's terrain navigation),
        // let it descend by not forcing the terrain-height command this tick.
        // Gated on height AND horizontal closeness - never height alone, and
        // never during attacks, stability recovery, or fallback.
        void ApplySheetOverride(const HeliState::Snapshot& s) {
            M.sheetWroteThisTick = false;
            // Unconditional ignore (restored option): the flag is forced on
            // every tick. The write is also masked out of the skid detector.
            if (gCfg.IgnoreHeliSheet) {
                if (HeliState::WriteSheetIgnore(true))
                    M.sheetWroteThisTick = true;
                return;
            }
            if (!gCfg.EnableSheetSafeOverride) return;
            if (M.state == AiState::Fallback || M.state == AiState::AttitudeRecovery) return;
            if (s.skidActive || K.active) return;

            const float above = s.pos[1] - s.dest[1];
            const float horiz = Len2(s.dest[0] - s.pos[0], s.dest[2] - s.pos[2]);
            const bool  horizClose = horiz < gCfg.SheetOverrideMaxHorizDistance;

            bool ignore = false;
            if (above > gCfg.SheetMaxHeightAboveTarget && horizClose)
                ignore = true;
            if (gCfg.SheetDescendWhenPlayerStopped && T.stopped && above > 8.0f && horizClose)
                ignore = true;

            if (ignore) {
                HeliState::WriteSheetIgnore(true);
                M.sheetWroteThisTick = true;
            }
        }

    } // namespace

    const char* AiStateName(AiState s) {
        switch (s) {
        case AiState::Arrival:            return "Arrival";
        case AiState::Chasing:            return "Chasing";
        case AiState::Attacking:          return "Attacking";
        case AiState::PostAttackRecovery: return "PostAttackRecovery";
        case AiState::StoppedPlayerHold:  return "StoppedPlayerHold";
        case AiState::DirectionRecovery:  return "DirectionRecovery";
        case AiState::CirclingRecovery:   return "CirclingRecovery";
        case AiState::StuckRecovery:      return "StuckRecovery";
        case AiState::AttitudeRecovery:   return "AttitudeRecovery";
        case AiState::Fallback:           return "Fallback";
        }
        return "?";
    }

    float SpeedAssistScale(const char** reasonOut) {
        const char* reason = "full";
        float scale = 1.0f;
        if (gStandDown) {
            scale = 0.0f; reason = "permanent stand-down";
        } else if (M.state == AiState::Fallback) {
            scale = 0.0f; reason = "fallback";
        } else if (A.active) {
            scale = 0.0f; reason = "attitude recovery";
        } else if (gCfg.EnableAttitudeStability
                   && A.reenableTime < gCfg.SpeedAssistReenableSeconds
                   && gCfg.SpeedAssistReenableSeconds > 0.01f) {
            scale = Clampf(A.reenableTime / gCfg.SpeedAssistReenableSeconds, 0.0f, 1.0f);
            reason = "post-recovery ramp";
        }
        if (reasonOut) *reasonOut = reason;
        return scale;
    }

    void GetAiDebug(AiDebug* out) {
        if (!out) return;
        out->state             = M.state;
        out->stateSeconds      = M.stateTime;
        out->targetSpeedEst    = T.targetSpeed;
        out->targetEstFrozen   = T.freezeTimer > 0.0f;
        out->physicalAngSpeedRadSec = A.angSpeed;
        out->behavHeadingRateDegSec = T.behavHeadingRateDegSec;
        out->destHeadingRateDegSec  = T.destHeadingRateDegSec;
        out->aggressionLevel   = gCfg.EnableDynamicAggression ? M.dynAggr : 1.0f;
        out->reattackTimer     = M.reattackTimer;
        out->backoffTimer      = M.backoffTimer;
        out->attacksObserved   = K.attacksObserved;
        out->engagements       = K.engagements;
        out->aborts            = K.aborts;
        out->unclassified      = K.unclassified;
        out->acceptedStarts    = K.acceptedEdges;
        out->rawPulses         = K.rawPulses;
        out->posEvidence       = K.posEvidence;
        out->falseGapTime      = K.timeSinceRawPulse;
        out->attackActive      = K.active;
        out->attackElapsed     = K.active ? K.lifeTime : 0.0f;
        out->heliPlayerDistance = (K.active && K.lastDistValid) ? K.lastDist : -1.0f;
        out->minHeliPlayerDistance = K.minDistValid ? K.minDist : -1.0f;
        out->distanceValid     = K.active && K.lastDistValid;
        out->passedTarget      = K.passedTarget;
        out->lastEndReason     = K.endReason[0] ? K.endReason : "none";
        out->classificationReason = K.classification[0] ? K.classification : "none";
        out->faults            = M.faults;
        out->playerStopped     = T.stopped;
        out->reversalDetected  = T.reversal;
        out->playerSource      = T.playerSrcActive;
        out->attitudeValid     = A.valid && A.upOrder != 0;
        out->upDot             = A.upDot;
        out->rollDeg           = A.rollDeg;
        out->pitchDeg          = A.pitchDeg;
        out->angSpeedRadSec    = A.angSpeed;
        out->attitudeStage     = A.active ? A.stage : 0;
        out->unstableSeconds   = A.unstableTime;
        out->suppressedTransitions = M.suppressedTransitions;
        out->suppressedAttackPulses = K.suppressedPulses;
        out->standDown         = gStandDown;
        out->commandedHeight             = gAlt.commanded;
        out->actualHeightAboveTarget     = gAlt.actualAbove;
        out->verticalVelocity            = gAlt.vertVel;
        out->dynAltContribution          = gAlt.dynContrib;
        out->recoveryHeightContribution  = gAlt.recovContrib;
        out->heightCommandClamped        = gAlt.commandClamped;
    }

    bool AiCoreEnabled() {
        if (gStandDown) return false;
        return gCfg.EnablePrediction || gCfg.EnableStationaryPlayer
            || gCfg.EnableDirectionChange || gCfg.EnableStuckRecovery
            || gCfg.EnableAntiCircling || gCfg.EnableAggression
            || gCfg.EnableDynamicAggression || gCfg.EnableDynamicAltitude
            || gCfg.EnableSheetSafeOverride || gCfg.EnableStateMachine
            || gCfg.EnableAttitudeStability
            || gCfg.FollowOnly || gCfg.AttackOnlyWhenAligned
            || gCfg.ReattackDelaySeconds > 0.0f || gCfg.MaxStrikeSeconds > 0.0f
            || gCfg.MinChaseSpeed > 0.0f || gCfg.MaxChaseSpeed > 0.0f;
    }

    // Concise, human-readable summary of the major systems that are enabled.
    void LogActiveSystems() {
        char buf[512];
        int n = 0;
        buf[0] = '\0';
        auto add = [&](bool on, const char* name) {
            if (!on) return;
            const size_t len = std::strlen(buf);
            std::snprintf(buf + len, sizeof(buf) - len, "%s%s", n ? ", " : "", name);
            ++n;
        };
        add(gCfg.EnableSpeedRegulator || gCfg.MinChaseSpeed > 0.0f || gCfg.MaxChaseSpeed > 0.0f,
            "speed control");
        add(gCfg.EnableAccelPatches, "acceleration");
        add(gCfg.EnableSteeringPatches, "steering");
        add(gCfg.EnableLeadPatches, "chase leading");
        add(gCfg.EnableAltitudePatches, "altitude");
        add(gCfg.EnableSkidEntryPatches || gCfg.EnableSkidStrikePatches, "skid attacks");
        add(gCfg.FollowOnly, "follow-only");
        add(gCfg.EnableAggression, "aggression");
        add(gCfg.EnableDynamicAggression, "dynamic aggression");
        add(gCfg.EnablePrediction, "player prediction");
        add(gCfg.EnableStationaryPlayer, "stopped-player handling");
        add(gCfg.EnableDirectionChange, "direction-change recovery");
        add(gCfg.EnableStuckRecovery, "stuck recovery");
        add(gCfg.EnableAntiCircling, "anti-circling");
        add(gCfg.EnableAttitudeStability, "stability protection");
        add(gCfg.EnableVisionPatches, "visibility");
        add(gCfg.EnableSheetSafeOverride, "altitude safe-override");
        add(gCfg.IgnoreHeliSheet, "terrain-sheet ignore");
        add(gCfg.DisableFuelBasedExit, "persistent helicopter");
        add(HeliRadioChat::Enabled(), "helicopter radio speech");
        if (n == 0)
            Log::Info("No optional systems enabled (helicopter behaves as in the base game).");
        else
            Log::Info("Enabled systems: %s.", buf);
    }

    void __cdecl AiTick(void* heliThis) {
        HeliState::Snapshot s;
        if (!HeliState::Capture(heliThis, &s)) {
            if (AiCoreEnabled()) Fault();
            return;
        }

        if (Registry::Enabled()) {
            Registry::NotifyDriving(s.heli, s.owner, s.rigidBody);
            Registry::UpdateKinematics(s.owner, s.pos, s.vel);
        }

        // Restored radio speech and the unconditional sheet ignore run even
        // when the rest of the dynamic AI layer is switched off.
        HeliRadioChat::Update(s);
        if (gCfg.IgnoreHeliSheet)
            HeliState::WriteSheetIgnore(true);

        if (!AiCoreEnabled()) return;

        const unsigned long now = GetTickCount();
        if (s.owner != T.ownerKey) {
            ResetAll(s.owner);
            FrameTime::Reset();
            Log::Info("[AI] tracking helicopter owner=%p (AI this=%p).", s.owner, s.heli);
            return;
        }

        // Observe the engine's own delta for free: the fuel timer counts down
        // by exactly one frame delta per update. Diagnostic only.
        if (T.fuelPrev > 1.0f && s.fuel > 0.0f) {
            const float gameDt = T.fuelPrev - s.fuel;
            if (gameDt > 0.0f && gameDt < 0.5f) FrameTime::NoteGameDelta(gameDt);
        }
        T.fuelPrev = s.fuel;

        // The engine's motion filters run on EVERY rendered frame, so they are
        // rescaled from the real frame time - not from the gated logic step.
        // (Using the gated step made this a no-op: the step always equals the
        // reference step, so the conversion factor was always exactly 1.)
        //
        // The STEADY delta is used, never the raw one. Measured frame times on
        // an uncapped game swing from 0.5 ms to 17 ms at a nominal 240 FPS, and
        // feeding that jitter into the filter constants swings them ~60x frame
        // to frame - heavily damped one frame, barely damped the next. That is
        // what makes the movement sway and then abruptly stiffen.
        if (gCfg.EnableFrameRateFix && gCfg.EnableSmoothingPatches) {
            const float realDt = FrameTime::SmoothedDelta();
            if (realDt > 0.0f) {
                if (gCfg.PatchOutputSmoothing)
                    FrameTime::ScaleFilterPair(gCfg.SmoothingOldWeight,
                                               gCfg.SmoothingFinalScale, realDt,
                                               &gLive.SmoothingOldWeight,
                                               &gLive.SmoothingFinalScale);
                if (gCfg.PatchDestVelFilter)
                    FrameTime::ScaleFilterPair(gCfg.DestVelFilterOldWeight,
                                               gCfg.DestVelFilterFinalScale, realDt,
                                               &gLive.DestVelFilterOldWeight,
                                               &gLive.DestVelFilterFinalScale);
            }
        }

        // Timestep for the mod's own logic. A zero step means this frame falls
        // between fixed reference-rate steps, which is normal pacing above the
        // reference rate - it is NOT a fault, so the estimator must not be
        // frozen and no state may advance.
        const float dt = FrameTime::Step();
        if (dt <= 0.0f) return;

        M.sheetWroteLastTick = M.sheetWroteThisTick;
        M.sheetWroteThisTick = false;

        if (gCfg.EnableDynamicAggression) {
            if (gCfg.DynAggrTimeToMaxMinutes > 0.01f)
                M.dynAggr += dt / (gCfg.DynAggrTimeToMaxMinutes * 60.0f);
            if (gCfg.DynAggrDecayPerMinute > 0.0f)
                M.dynAggr -= gCfg.DynAggrDecayPerMinute / 60.0f * dt;
            M.dynAggr = Clampf(M.dynAggr, gCfg.DynAggrMin, gCfg.DynAggrMax);
        }

        UpdateTracker(s, dt);
        UpdateAttitude(s, dt);
        UpdateAttackObservation(s, dt);
        UpdateStateMachine(dt);

        if (gCfg.AiUpdateIntervalMs <= 0
            || now - gLastAiUpdateMs >= static_cast<unsigned long>(gCfg.AiUpdateIntervalMs)) {
            gLastAiUpdateMs = now;
            ApplyDynamicTuning(s, dt);
        }
        ApplyDriveSpeedShaping(s);
        ApplyAttitudeDamping(s, dt);
        ApplySheetOverride(s);
    }

} // namespace Systems
