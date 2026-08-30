// AiCore.h - the dynamic helicopter AI layer.
//
// Verified mechanisms only:
//  1. Live redirected constants (gLive.*) - retuned per tick with slew
//     limiting and dead zones.
//  2. The AI desired drive speed (this+0x84) at OnDriving entry.
//  3. Per-tick conditional bIgnoreHeliSheet override (self-writes masked
//     out of the skid detector).
//  4. Rigid-body horizontal velocity writes.
//  5. Rigid-body forward/right vectors for attitude stability (angular
//     speed DERIVED from orientation deltas).
//  6. Live accel-budget data writes (0x008F8DCC/D0) for per-state scaling.
//
// The helicopter is NEVER teleported and its transform is never written.
#pragma once

namespace Systems {

    enum class AiState {
        Arrival,
        Chasing,
        Attacking,
        PostAttackRecovery,
        StoppedPlayerHold,
        DirectionRecovery,
        CirclingRecovery,
        StuckRecovery,
        AttitudeRecovery,
        Fallback
    };
    const char* AiStateName(AiState s);

    struct AiDebug {
        AiState state;
        float   stateSeconds;
        float   targetSpeedEst;
        bool    targetEstFrozen;
        // Three DISTINCT, clearly-named turn metrics (were conflated as "ownTurn"):
        float   physicalAngSpeedRadSec;   // real body rotation (attitude, rad/s)
        float   behavHeadingRateDegSec;   // heli velocity-direction change (deg/s)
        float   destHeadingRateDegSec;    // target velocity-direction change (deg/s)
        float   aggressionLevel;
        float   reattackTimer;
        float   backoffTimer;
        int     attacksObserved;
        int     engagements;
        int     aborts;
        int     unclassified;
        int     acceptedStarts;
        unsigned rawPulses;
        float   posEvidence;
        float   falseGapTime;
        // attack-lifecycle tracking (player-distance based)
        bool    attackActive;
        float   attackElapsed;
        float   heliPlayerDistance;
        float   minHeliPlayerDistance;
        bool    distanceValid;
        bool    passedTarget;
        const char* lastEndReason;
        const char* classificationReason;
        int     faults;
        bool    playerStopped;
        bool    reversalDetected;
        bool    playerSource;        // target motion from the real player rigid body
        bool    attitudeValid;
        float   upDot;
        float   rollDeg;
        float   pitchDeg;
        float   angSpeedRadSec;      // == physicalAngSpeedRadSec (kept for compat)
        int     attitudeStage;
        float   unstableSeconds;
        unsigned suppressedTransitions;
        unsigned suppressedAttackPulses;
        bool    standDown;           // permanent session stand-down active
        // altitude controller diagnostics
        float   commandedHeight;
        float   actualHeightAboveTarget;
        float   verticalVelocity;
        float   dynAltContribution;
        float   recoveryHeightContribution;
        bool    heightCommandClamped;
    };
    void GetAiDebug(AiDebug* out);

    // Speed-assist gate: 0 = suspended, 1 = full, in between = re-enable ramp.
    float SpeedAssistScale(const char** reasonOut);

    void __cdecl AiTick(void* heliThis);
    bool AiCoreEnabled();

} // namespace Systems
