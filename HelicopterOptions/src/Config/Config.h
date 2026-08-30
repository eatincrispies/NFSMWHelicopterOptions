// Config.h - all mod settings (modular multi-file configuration).
//
// Files live in  scripts\HelicopterOptions\Configuration\  and are loaded by
// the table-driven parser in Ini.cpp; the key table there is the single
// source of truth for file ownership, types, bounds and defaults. Every
// field below is consumed by runtime code - no placeholders.
#pragma once
#include <cstdint>
#include "../Core/Addresses.h"

struct Config {

    // ===== GeneralSettings.ini ==============================================
    // [General]
    bool LogToFile            = true;
    bool VerboseLogging       = false;
    bool VerboseConfigLogging = false;   // log every applied key while loading
    int  MaxLogSizeKB         = 4096;    // session rotation threshold (.old)
    bool UnknownKeyWarnings   = true;
    bool AutoCreateMissingFiles = true;  // regenerate a MISSING file from defaults
    bool StartupSummary       = true;    // per-file section/key summary lines
    int  ConfigVersion        = 2;       // mismatch produces a warning
    // [Compatibility]
    bool AllowUnsupportedExe  = false;

    // ===== Leading.ini ======================================================
    // [ChopperLead]
    bool  EnableLeadPatches  = false;
    float LeadSpeedScale     = Addr::Vanilla::LeadSpeedScale;     // 0.4
    float LeadBase           = Addr::Vanilla::LeadBase;           // 30
    float LeadMin            = 0.0f;     // floor on the dynamic lead base
    float LeadMax            = Addr::Vanilla::LeadMax;            // 45
    float LeadSkidMultiplier = Addr::Vanilla::LeadSkidMultiplier; // 0.75
    float ArrivalLeadScale   = 1.0f;     // lead multiplier during Arrival state
    float LeadSlewBaseRate   = 40.0f;    // m/s of base-lead change
    float LeadSlewScaleRate  = 0.8f;     // per-second change of the speed scale
    float LeadDeadZone       = 0.5f;     // skip live writes for tiny deltas (m)

    // ===== Altitude.ini =====================================================
    // [ChopperAltitude]
    bool  EnableAltitudePatches = false;
    float ChaseHeightSkid    = Addr::Vanilla::ChaseHeightSkid;    // 2
    float ChaseHeightClose   = Addr::Vanilla::ChaseHeightClose;   // 6
    float ChaseHeightHigh    = Addr::Vanilla::ChaseHeightHigh;    // 12
    bool  EnableDynamicAltitude = false;
    float DynAltHighSpeedBoost = 3.0f;
    float DynAltHighSpeedThreshold = 60.0f;
    float TurnHeightBoost    = 0.0f;     // +m while own turn rate is high
    float ArrivalHeightBoost = 0.0f;     // +m during Arrival state
    float MinCommandedHeight = -5.0f;    // clamps on the final live heights
    float MaxCommandedHeight = 50.0f;
    float HeightSlewRate     = 20.0f;    // m/s of live height change
    // [AttitudeStability]
    bool  EnableAttitudeStability   = true;
    float MaximumSafeRollDegrees    = 45.0f;
    float MaximumSafePitchDegrees   = 35.0f;
    float MaximumAngularSpeed       = 3.5f;
    float UnstableDetectSeconds     = 0.35f;
    float RecoveryReleaseDegrees    = 15.0f;
    float RecoveryStableSeconds     = 0.75f;
    float SpeedAssistReenableSeconds = 1.5f;
    float Stage2Seconds             = 2.0f;
    float Stage3Seconds             = 5.0f;
    float Stage1SpeedCut            = 0.50f;
    float Stage2SpeedCut            = 0.35f;
    float Stage3SpeedCut            = 0.25f;
    float Stage2Damping             = 0.94f;   // per-tick horizontal velocity factor
    float Stage3Damping             = 0.90f;
    float RecoveryHeightBoost       = 6.0f;
    bool  LogAttitudeEvents         = true;

    // ===== Skids.ini ========================================================
    // [SkidEntry]
    bool  EnableSkidEntryPatches = false;
    float SkidCooldownThreshold  = Addr::Vanilla::SkidCooldown;
    float SkidEntryMaxDistance   = Addr::Vanilla::EntryMaxDistance;
    float SkidEntryMinDistance   = Addr::Vanilla::EntryMinDistance;
    float SkidEntryAlignmentDot  = Addr::Vanilla::EntryAlignmentDot;
    float SkidEntryMaxHeightDelta = Addr::Vanilla::EntryMaxHeightDelta;
    bool  ForceSkidHitAttribute  = false;
    // [SkidStrike]
    bool  EnableSkidStrikePatches = false;
    float SkidSideOffset          = Addr::Vanilla::SideOffsetPos;
    float SkidApproachVelocityLead = Addr::Vanilla::ApproachVelocityLead;
    float SkidApproachHeight      = Addr::Vanilla::ApproachHeight;
    float SkidLowExtraHeight      = Addr::Vanilla::LowExtraHeight;
    float SkidStrikeStartDistance = Addr::Vanilla::StrikeStartDistance;
    float SkidStrikeLateralTriggerDistance = Addr::Vanilla::StrikeLateralTrigger;
    float SkidStrikeVelocityLead  = Addr::Vanilla::StrikeVelocityLead;
    float SkidStrikeBackScale     = Addr::Vanilla::StrikeBackScale;
    float SkidAbortDistanceAheadSq  = Addr::Vanilla::AbortAheadSq;
    float SkidAbortDistanceBehindSq = Addr::Vanilla::AbortBehindSq;
    // [SkidAttack]
    bool  FollowOnly            = false;
    bool  AttackOnlyWhenAligned = false;
    float AlignedModeDot        = 0.85f;
    float ReattackDelaySeconds  = 0.0f;
    float MaxStrikeSeconds      = 0.0f;
    bool  LogAttackEvents       = true;
    float AttackStartDebounceSeconds = 0.10f;
    float AttackEndDebounceSeconds   = 0.25f;

    // ===== Acceleration.ini =================================================
    // [ChopperAcceleration]
    bool  EnableAccelPatches   = false;
    float AccelBudgetMax       = Addr::Vanilla::MaxChopperAccel;
    float AccelBudgetMin       = Addr::Vanilla::MinChopperAccel;
    float VelocityErrorToAccelRatio = Addr::Vanilla::ChopperRatio;
    float AccelBudgetSpeedScale = Addr::Vanilla::AccelBudgetSpeedScale;
    bool  ForceMaxAccelAuthority = false;
    float AttackBudgetScale    = 1.0f;   // live budget multiplier while Attacking
    float RecoveryBudgetScale  = 1.0f;   // live budget multiplier in recovery states
    float BudgetSlewRate       = 200.0f; // units/s of live budget change

    // ===== SteeringControl.ini ==============================================
    // [ChopperSteering]
    bool  EnableSteeringPatches = false;
    float TurnResponseScale     = Addr::Vanilla::TurnResponseScale;
    float TurnClamp             = Addr::Vanilla::TurnClampPos;
    float HighSpeedTurnClampScale = 1.0f;   // clamp multiplier above threshold
    float HighSpeedClampThreshold = 70.0f;  // m/s
    float LowSpeedTurnClampScale  = 1.0f;   // clamp multiplier below threshold
    float LowSpeedClampThreshold  = 15.0f;  // m/s
    float AttackTurnClampScale    = 1.0f;   // clamp multiplier while Attacking
    float SteeringSlewSeconds     = 0.25f;  // blend time for live clamp changes
    // [ChopperSmoothing]
    bool  EnableSmoothingPatches   = false;
    bool  PatchOutputSmoothing     = true;
    float SmoothingOldWeight       = Addr::Vanilla::SmoothingOldWeight;
    float SmoothingFinalScale      = Addr::Vanilla::SmoothingFinalScale;
    bool  PatchDestVelFilter       = true;
    float DestVelFilterOldWeight   = Addr::Vanilla::DestVelFilterWeight;
    float DestVelFilterFinalScale  = Addr::Vanilla::DestVelFilterScale;
    bool  AllowUnstableFilterGain  = false;
    bool  LogFilterGains           = true;

    // ===== Speed.ini ========================================================
    // [ChopperSpeed]
    bool  EnableSpeedRegulator  = false;
    float SpeedFactor           = 1.0f;
    float SpeedMax              = 0.0f;
    float SpeedPush             = 0.05f;
    float OverspeedBrake        = 0.10f;
    float MinApplySpeed         = 20.0f;
    bool  TurnSlowdown          = true;
    float TurnSlowdownStrength  = 120.0f;
    bool  SkipDuringSkid        = true;
    float MinChaseSpeed         = 0.0f;
    float MaxChaseSpeed         = 0.0f;
    int   VerboseIntervalMs     = 1000;
    float ArrivalSpeedScale     = 1.0f;  // desired-speed multiplier per state
    float StoppedSpeedScale     = 1.0f;
    float RecoverySpeedScale    = 1.0f;
    float ReversalSpeedScale    = 1.0f;

    // ===== Visibility.ini ===================================================
    // [ChopperVision]
    bool  EnableVisionPatches = false;
    bool  SeeThroughWalls     = false;

    // ===== Predictions.ini ==================================================
    // [Prediction]
    bool  EnablePrediction    = false;
    // Prefer the REAL local-player rigid body (verified IPlayer chain) as the
    // target-motion source; drive-target differences remain the fallback.
    bool  UsePlayerPositionSource = true;
    float PredictionLeadScale = 1.0f;
    float TurnLeadReduction   = 0.5f;
    float BrakeLeadReduction  = 0.5f;
    float HighSpeedLeadBoost  = 0.15f;
    float HighSpeedThreshold  = 55.0f;
    float StablePathSeconds   = 2.0f;
    int   MinValidSamples     = 10;      // confidence gate for detections
    float FreezeSeconds       = 0.5f;    // estimate hold time across target jumps
    float TeleportJumpDistance = 12.0f;  // minimum jump treated as discontinuity
    float MaxTargetAcceleration = 60.0f; // m/s^2 clamp on the accel estimate
    bool  LogEstimatorEvents  = false;   // rate-limited freeze/unfreeze logs
    // [StationaryPlayer]
    bool  EnableStationaryPlayer   = false;
    float StoppedSpeedThreshold    = 3.0f;
    float StoppedDetectSeconds     = 1.5f;
    float StoppedLeadScale         = 0.3f;
    bool  SuppressAttacksWhenStopped = false;
    int   MovingClearSamples       = 2;
    // [StateMachine]
    bool  EnableStateMachine   = false;
    bool  LogStateTransitions  = true;
    float StateTimeoutSeconds  = 30.0f;
    float TransitionLogCooldownSeconds = 2.5f;

    // ===== Recovery.ini =====================================================
    // [DirectionChangeRecovery]
    bool  EnableDirectionChange    = false;
    float ReversalDot              = -0.5f;
    float SharpTurnDegPerSec       = 90.0f;
    float DirectionRecoverySeconds = 1.5f;
    float ReversalLeadReduction    = 0.7f;
    bool  AbortAttackOnReversal    = true;
    float DirectionCooldownSeconds = 0.0f;
    // [StuckRecovery]
    bool  EnableStuckRecovery      = false;
    float StuckMinProgressSpeed    = 4.0f;
    float StuckNoProgressSeconds   = 4.0f;
    float StuckRecoverySeconds     = 3.0f;
    float StuckRecoveryLeadScale   = 0.5f;
    float StuckRecoveryExtraHeight = 6.0f;
    float StuckRecoveryCooldownSeconds = 10.0f;
    // [AntiCircling]
    bool  EnableAntiCircling      = false;
    float CirclingTurnDegPerSec   = 70.0f;
    float CirclingDetectSeconds   = 3.0f;
    float OscillationFlipsPerSec  = 3.0f;
    float CirclingRecoverySeconds = 2.0f;
    float CirclingCooldownSeconds = 0.0f;

    // ===== Aggression.ini ===================================================
    // [Aggression]
    bool  EnableAggression     = false;
    float AttackFrequency      = 1.0f;
    float AttackWillingness    = 1.0f;
    float AlignmentTolerance   = 0.0f;
    float CatchUpUrgency       = 0.0f;
    float CatchUpStartDistance = 120.0f;
    float CatchUpMaxSpeedBoost = 0.30f;
    float FailedAttackBackoffSeconds = 3.0f;
    float ArrivalGraceSeconds  = 5.0f;
    // [DynamicAggression]
    bool  EnableDynamicAggression   = false;
    float DynAggrMin                = 0.0f;
    float DynAggrMax                = 1.0f;
    float DynAggrTimeToMaxMinutes   = 3.0f;
    float DynAggrFailedAttackIncrease = 0.10f;
    bool  DynAggrResetOnEngagement  = true;
    float DynAggrDecayPerMinute     = 0.0f;   // passive decay toward MinLevel

    // ===== AIHelicopterBehavior.ini =========================================
    // [FallbackBehavior]
    bool  EnableFallback           = true;
    float FallbackSeconds          = 5.0f;
    int   FallbackMaxFaultsPerMinute = 10;
    int   PermanentStandDownFaults = 200;    // total faults -> dynamic layer off for session
    // [ExitBehavior]
    bool  DisableFuelBasedExit   = false;
    bool  EnableExitActionPatches = false;
    bool  PatchExitFlySpeed      = false;
    bool  PatchExitSeekUpThreshold = false;
    bool  PatchExitSeekAheadDistance = false;
    bool  PatchExitSeekCarHeight = false;
    bool  PatchExitReachDistance = false;
    bool  PatchExitFlyout        = false;
    bool  PatchExitFinishRules   = false;
    bool  PatchExitDoDrivingMode = false;
    float ExitFlySpeed           = Addr::Vanilla::ExitFlySpeed;
    float ExitSeekUpThreshold    = Addr::Vanilla::ExitSeekUpThreshold;
    float ExitSeekAheadDistance  = Addr::Vanilla::ExitSeekAhead;
    float ExitSeekCarReachDistanceSq = Addr::Vanilla::ExitReachDistanceSq;
    float ExitRightScale         = Addr::Vanilla::ExitRightScale;
    float ExitFlyoutBackScale    = Addr::Vanilla::ExitFlyoutBackScale;
    float ExitFlyoutExtraHeight  = Addr::Vanilla::ExitFlyoutExtraHeight;
    float ExitTargetHeight       = Addr::Vanilla::ExitHeight;
    float ExitFinishDistanceSq   = Addr::Vanilla::ExitFinishDistanceSq;
    uint8_t ExitDoDrivingMode    = 7;

    // ===== Navigation.ini ===================================================
    // [HeliSheet]
    bool  EnableSheetSafeOverride = false;
    float SheetMaxHeightAboveTarget = 25.0f;
    float SheetOverrideMaxHorizDistance = 60.0f;
    bool  SheetDescendWhenPlayerStopped = true;
    bool  RespectHeliSheetDuringSkid = false;
    // Unconditional per-tick sheet-ignore (the V1-era global ignore, restored
    // by request). Note: the sheet flag doubles as the skid observable, so
    // this blinds the mod's own attack-event detection while enabled.
    bool  IgnoreHeliSheet = false;

    // ===== Radio.ini ========================================================
    // [HeliRadioChat] - restored helicopter police-radio speech.
    bool  EnableRadioChat = true;
    bool  RadioAnnounceArrival = true;
    bool  RadioSpotterCalls = true;
    bool  RadioLostVisualCalls = true;
    float RadioLostVisualDistance = 180.0f;
    float RadioLostVisualSeconds = 4.0f;
    bool  RadioPositionCalls = true;
    float RadioStoppedCallSeconds = 3.0f;
    bool  RadioFuelCalls = true;
    float RadioFuelWarnSeconds = 15.0f;
    bool  RadioArrestCalls = true;
    float RadioArrestDistance = 40.0f;
    float RadioArrestSeconds = 4.0f;
    bool  RadioAnnounceAttacks = false;
    bool  RadioHazardCalls = false;
    int   RadioHazardContext = 0;
    bool  RadioSwarmCalls = false;
    float RadioGlobalCooldownSeconds = 8.0f;
    float RadioEventCooldownSeconds = 25.0f;
    int   RadioMaxLinesPerMinute = 6;
    int   RadioHeliSpeakerRole = 2;
    bool  RadioLogEvents = true;

    // ===== Debug.ini ========================================================
    // [Telemetry]
    bool  EnableTelemetry   = false;
    int   TelemetryIntervalMs = 1000;
    bool  TelemetryAiState = true;
    bool  LogHeliLifecycle = true;
    // [ChopperSpawner] - experimental dispatch gates. The game only tracks one
    // helicopter at a time, so extra helicopters can corrupt state on despawn.
    // Off by default; for experimentation only.
    bool  EnableDispatchPatches = false;
    bool  AllowCopheliWeightedSelection = false;
    bool  IgnoreExistingHeliForSelectorTrigger = false;
    bool  IgnoreExistingHeliForSpecialSpawn = false;
    bool  ForceWeightedSelectorAlwaysReturnCopheli = false;
    bool  BypassSpawnCapForImmediateRequests = false;
    bool  LogSpawnEvents = true;
    // [ChopperCollision] - proximity telemetry between simultaneously alive
    // helicopters (meaningful only during spawner research sessions).
    bool  EnableProximityTelemetry = true;
    float CollisionWarnDistance    = 25.0f;
    float CollisionPredictSeconds  = 2.0f;
    int   CollisionLogIntervalMs   = 1000;
    // [Diagnostics]
    int   AiUpdateIntervalMs = 0;   // dynamic-tuning recompute interval (0 = every tick)
    // ===== GeneralSettings.ini [FrameRate] ==================================
    bool  EnableFrameRateFix   = true;   // compensate when the game runs above the rate below
    float HelicopterUpdateRate = 60.0f;  // the rate the helicopter behaves as if running at
    bool  LogFrameRate         = false;  // periodic frame-timing line
};

extern Config gCfg;

// Frame-rate compensation internals. Deliberately not exposed as settings:
// there is no sensible reason for a player to change either one.
//  kFrameMaxStepSeconds  longest delta integrated in one go (stall protection)
//  kFrameMinPhysicsDelta replaces the engine's 0.005 s (200 FPS) cutoff below
//                        which it stops deriving the helicopter's own velocity
constexpr float kFrameMaxStepSeconds  = 0.25f;
constexpr float kFrameMinPhysicsDelta = 0.0001f;

// DLL-owned floats read by the patched game instructions.
struct LiveFloats {
    float SkidCooldownThreshold, SkidEntryMaxDistance, SkidEntryMinDistance,
          SkidEntryAlignmentDot, SkidEntryMaxHeightDelta;
    float LeadSpeedScale, LeadBase, LeadMax, LeadSkidMultiplier;
    float ChaseHeightSkid, ChaseHeightClose, ChaseHeightHigh;
    float SideOffsetPos, SideOffsetNeg, ApproachVelocityLead, ApproachHeight,
          LowExtraHeight, StrikeStartDistance, StrikeLateralTrigger,
          StrikeVelocityLead, StrikeBackScale, AbortAheadSq, AbortBehindSq;
    float DestVelFilterOldWeight, DestVelFilterFinalScale;
    float AccelBudgetSpeedScale, TurnResponseScale, TurnClampPos, TurnClampNeg,
          SmoothingOldWeight, SmoothingFinalScale;
    float ChopperVelDtGate;
    float ExitSeekUpThreshold, ExitSeekAheadDistance, ExitSeekCarReachDistanceSq,
          ExitRightScale, ExitFlyoutBackScale, ExitFlyoutExtraHeight,
          ExitTargetHeight, ExitFinishDistanceSq;
};
extern LiveFloats gLive;

void SyncLiveFloats();
