// Ini.cpp - table-driven modular configuration loader.
//
// Fifteen INI files in  <moduleDir>\HelicopterOptions\Configuration\  are
// loaded in a fixed order. The key table below is the single source of
// truth: file ownership, section ownership, type, bounds and default of
// every setting. That makes unknown-key warnings, misplaced-section
// rejection, duplicate detection, startup statistics and missing-file
// regeneration all mechanical.
//
// Parser properties: UTF-8 (BOM tolerated), CRLF/LF, blank lines, full-line
// ';' and '#' comments, case-insensitive sections/keys, strict numeric
// parsing (trailing garbage rejected), 0/1 booleans, clamped ranges with
// warnings, first-value-wins duplicates. One malformed file never affects
// the others; one malformed key never affects the rest of its file.
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cfloat>
#include "Ini.h"
#include "Config.h"
#include "Validation.h"
#include "../Core/Log.h"

Config gCfg;
LiveFloats gLive;

namespace Ini {

    namespace {

        // ---------------------------------------------------------------- files
        enum FileId {
            F_GENERAL, F_LEADING, F_ALTITUDE, F_SKIDS, F_ACCEL, F_STEER,
            F_SPEED, F_VIS, F_PRED, F_RECOV, F_AGGR, F_BEHAV, F_NAV, F_RADIO,
            F_DEBUG,
            F_COUNT
        };
        const char* kFileNames[F_COUNT] = {
            "GeneralSettings.ini", "Leading.ini", "Altitude.ini", "Skids.ini",
            "Acceleration.ini", "SteeringControl.ini", "Speed.ini",
            "Visibility.ini", "Predictions.ini", "Recovery.ini",
            "Aggression.ini", "AIHelicopterBehavior.ini", "Navigation.ini",
            "Radio.ini", "Debug.ini"
        };

        enum KeyType { T_BOOL, T_FLOAT, T_INT, T_U8 };
        struct KeyDef {
            uint8_t     file;
            const char* section;
            const char* key;
            uint8_t     type;
            void*       ptr;
            float       mn, mx;      // numeric bounds (ints use mn/mx too)
        };

        #define KB(f, s, k, field)             { f, s, k, T_BOOL,  &gCfg.field, 0, 1 }
        #define KF(f, s, k, field, lo, hi)     { f, s, k, T_FLOAT, &gCfg.field, lo, hi }
        #define KI(f, s, k, field, lo, hi)     { f, s, k, T_INT,   &gCfg.field, lo, hi }
        #define KU8(f, s, k, field, lo, hi)    { f, s, k, T_U8,    &gCfg.field, lo, hi }

        const KeyDef kKeys[] = {
        // ---- GeneralSettings.ini ------------------------------------------
        KB (F_GENERAL, "General", "LogToFile",            LogToFile),
        KB (F_GENERAL, "General", "VerboseLogging",       VerboseLogging),
        KB (F_GENERAL, "General", "VerboseConfigLogging", VerboseConfigLogging),
        KI (F_GENERAL, "General", "MaxLogSizeKB",         MaxLogSizeKB, 256, 65536),
        KB (F_GENERAL, "General", "UnknownKeyWarnings",   UnknownKeyWarnings),
        KB (F_GENERAL, "General", "AutoCreateMissingFiles", AutoCreateMissingFiles),
        KB (F_GENERAL, "General", "StartupSummary",       StartupSummary),
        KI (F_GENERAL, "General", "ConfigVersion",        ConfigVersion, 0, 100),
        KB (F_GENERAL, "Compatibility", "AllowUnsupportedExe", AllowUnsupportedExe),
        KB (F_GENERAL, "FrameRate", "Enable",               EnableFrameRateFix),
        KF (F_GENERAL, "FrameRate", "HelicopterUpdateRate", HelicopterUpdateRate, 20.0f, 120.0f),
        KB (F_GENERAL, "FrameRate", "LogFrameRate",         LogFrameRate),
        // ---- Leading.ini ----------------------------------------------------
        KB (F_LEADING, "ChopperLead", "Enable",             EnableLeadPatches),
        KF (F_LEADING, "ChopperLead", "LeadSpeedScale",     LeadSpeedScale, 0.0f, 2.0f),
        KF (F_LEADING, "ChopperLead", "LeadBase",           LeadBase, 0.0f, 100.0f),
        KF (F_LEADING, "ChopperLead", "LeadMin",            LeadMin, 0.0f, 60.0f),
        KF (F_LEADING, "ChopperLead", "LeadMax",            LeadMax, 5.0f, 150.0f),
        KF (F_LEADING, "ChopperLead", "LeadSkidMultiplier", LeadSkidMultiplier, 0.1f, 2.0f),
        KF (F_LEADING, "ChopperLead", "ArrivalLeadScale",   ArrivalLeadScale, 0.2f, 2.0f),
        KF (F_LEADING, "ChopperLead", "LeadSlewBaseRate",   LeadSlewBaseRate, 5.0f, 200.0f),
        KF (F_LEADING, "ChopperLead", "LeadSlewScaleRate",  LeadSlewScaleRate, 0.1f, 5.0f),
        KF (F_LEADING, "ChopperLead", "LeadDeadZone",       LeadDeadZone, 0.0f, 5.0f),
        // ---- Altitude.ini ------------------------------------------------------
        KB (F_ALTITUDE, "ChopperAltitude", "Enable",              EnableAltitudePatches),
        KF (F_ALTITUDE, "ChopperAltitude", "ChaseHeightSkid",     ChaseHeightSkid, -5.0f, 30.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "ChaseHeightClose",    ChaseHeightClose, -5.0f, 30.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "ChaseHeightHigh",     ChaseHeightHigh, -5.0f, 50.0f),
        KB (F_ALTITUDE, "ChopperAltitude", "EnableDynamicAltitude", EnableDynamicAltitude),
        KF (F_ALTITUDE, "ChopperAltitude", "HighSpeedHeightBoost", DynAltHighSpeedBoost, 0.0f, 15.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "HighSpeedThreshold",  DynAltHighSpeedThreshold, 10.0f, 200.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "TurnHeightBoost",     TurnHeightBoost, 0.0f, 15.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "ArrivalHeightBoost",  ArrivalHeightBoost, 0.0f, 20.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "MinCommandedHeight",  MinCommandedHeight, -5.0f, 30.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "MaxCommandedHeight",  MaxCommandedHeight, 5.0f, 50.0f),
        KF (F_ALTITUDE, "ChopperAltitude", "HeightSlewRate",      HeightSlewRate, 2.0f, 100.0f),
        KB (F_ALTITUDE, "AttitudeStability", "Enable",                 EnableAttitudeStability),
        KF (F_ALTITUDE, "AttitudeStability", "MaximumSafeRollDegrees", MaximumSafeRollDegrees, 20.0f, 85.0f),
        KF (F_ALTITUDE, "AttitudeStability", "MaximumSafePitchDegrees", MaximumSafePitchDegrees, 15.0f, 85.0f),
        KF (F_ALTITUDE, "AttitudeStability", "MaximumAngularSpeed",    MaximumAngularSpeed, 1.0f, 20.0f),
        KF (F_ALTITUDE, "AttitudeStability", "UnstableDetectSeconds",  UnstableDetectSeconds, 0.1f, 3.0f),
        KF (F_ALTITUDE, "AttitudeStability", "RecoveryReleaseDegrees", RecoveryReleaseDegrees, 5.0f, 45.0f),
        KF (F_ALTITUDE, "AttitudeStability", "RecoveryStableSeconds",  RecoveryStableSeconds, 0.2f, 5.0f),
        KF (F_ALTITUDE, "AttitudeStability", "SpeedAssistReenableSeconds", SpeedAssistReenableSeconds, 0.0f, 10.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage2Seconds",          Stage2Seconds, 0.5f, 20.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage3Seconds",          Stage3Seconds, 1.0f, 30.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage1SpeedCut",         Stage1SpeedCut, 0.1f, 1.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage2SpeedCut",         Stage2SpeedCut, 0.1f, 1.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage3SpeedCut",         Stage3SpeedCut, 0.05f, 1.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage2Damping",          Stage2Damping, 0.80f, 1.0f),
        KF (F_ALTITUDE, "AttitudeStability", "Stage3Damping",          Stage3Damping, 0.75f, 1.0f),
        KF (F_ALTITUDE, "AttitudeStability", "RecoveryHeightBoost",    RecoveryHeightBoost, 0.0f, 20.0f),
        KB (F_ALTITUDE, "AttitudeStability", "LogAttitudeEvents",      LogAttitudeEvents),
        // ---- Skids.ini --------------------------------------------------------------
        KB (F_SKIDS, "SkidEntry", "Enable",                 EnableSkidEntryPatches),
        KF (F_SKIDS, "SkidEntry", "SkidCooldownThreshold",  SkidCooldownThreshold, -10.0f, 1.0f),
        KF (F_SKIDS, "SkidEntry", "SkidEntryMinDistance",   SkidEntryMinDistance, 0.0f, 25.0f),
        KF (F_SKIDS, "SkidEntry", "SkidEntryMaxDistance",   SkidEntryMaxDistance, 5.0f, 150.0f),
        KF (F_SKIDS, "SkidEntry", "SkidEntryAlignmentDot",  SkidEntryAlignmentDot, -1.0f, 0.99f),
        KF (F_SKIDS, "SkidEntry", "SkidEntryMaxHeightDelta", SkidEntryMaxHeightDelta, 2.0f, 60.0f),
        KB (F_SKIDS, "SkidEntry", "ForceSkidHitAttribute",  ForceSkidHitAttribute),
        KB (F_SKIDS, "SkidStrike", "Enable",                     EnableSkidStrikePatches),
        KF (F_SKIDS, "SkidStrike", "SkidSideOffset",             SkidSideOffset, 0.0f, 12.0f),
        KF (F_SKIDS, "SkidStrike", "SkidApproachVelocityLead",   SkidApproachVelocityLead, 0.0f, 1.0f),
        KF (F_SKIDS, "SkidStrike", "SkidApproachHeight",         SkidApproachHeight, -5.0f, 15.0f),
        KF (F_SKIDS, "SkidStrike", "SkidLowExtraHeight",         SkidLowExtraHeight, 0.0f, 15.0f),
        KF (F_SKIDS, "SkidStrike", "SkidStrikeStartDistance",    SkidStrikeStartDistance, 1.0f, 30.0f),
        KF (F_SKIDS, "SkidStrike", "SkidStrikeLateralTriggerDistance", SkidStrikeLateralTriggerDistance, 0.0f, 10.0f),
        KF (F_SKIDS, "SkidStrike", "SkidStrikeVelocityLead",     SkidStrikeVelocityLead, 0.0f, 1.0f),
        KF (F_SKIDS, "SkidStrike", "SkidStrikeBackScale",        SkidStrikeBackScale, -2.0f, 2.0f),
        KF (F_SKIDS, "SkidStrike", "SkidAbortDistanceAheadSq",   SkidAbortDistanceAheadSq, 100.0f, 10000.0f),
        KF (F_SKIDS, "SkidStrike", "SkidAbortDistanceBehindSq",  SkidAbortDistanceBehindSq, 25.0f, 5000.0f),
        KB (F_SKIDS, "SkidAttack", "FollowOnly",             FollowOnly),
        KB (F_SKIDS, "SkidAttack", "AttackOnlyWhenAligned",  AttackOnlyWhenAligned),
        KF (F_SKIDS, "SkidAttack", "AlignedModeDot",         AlignedModeDot, 0.5f, 0.99f),
        KF (F_SKIDS, "SkidAttack", "ReattackDelaySeconds",   ReattackDelaySeconds, 0.0f, 120.0f),
        KF (F_SKIDS, "SkidAttack", "MaxStrikeSeconds",       MaxStrikeSeconds, 0.0f, 60.0f),
        KB (F_SKIDS, "SkidAttack", "LogAttackEvents",        LogAttackEvents),
        KF (F_SKIDS, "SkidAttack", "AttackStartDebounceSeconds", AttackStartDebounceSeconds, 0.03f, 1.0f),
        KF (F_SKIDS, "SkidAttack", "AttackEndDebounceSeconds",   AttackEndDebounceSeconds, 0.05f, 2.0f),
        // ---- Acceleration.ini ------------------------------------------------------------
        KB (F_ACCEL, "ChopperAcceleration", "Enable",                    EnableAccelPatches),
        KF (F_ACCEL, "ChopperAcceleration", "AccelBudgetMax",            AccelBudgetMax, 40.0f, 250.0f),
        KF (F_ACCEL, "ChopperAcceleration", "AccelBudgetMin",            AccelBudgetMin, 0.0f, 160.0f),
        KF (F_ACCEL, "ChopperAcceleration", "VelocityErrorToAccelRatio", VelocityErrorToAccelRatio, 0.5f, 8.0f),
        KF (F_ACCEL, "ChopperAcceleration", "AccelBudgetSpeedScale",     AccelBudgetSpeedScale, 0.0f, 3.0f),
        KB (F_ACCEL, "ChopperAcceleration", "ForceMaxAccelAuthority",    ForceMaxAccelAuthority),
        KF (F_ACCEL, "ChopperAcceleration", "AttackBudgetScale",         AttackBudgetScale, 0.3f, 2.0f),
        KF (F_ACCEL, "ChopperAcceleration", "RecoveryBudgetScale",       RecoveryBudgetScale, 0.3f, 2.0f),
        KF (F_ACCEL, "ChopperAcceleration", "BudgetSlewRate",            BudgetSlewRate, 20.0f, 1000.0f),
        // ---- SteeringControl.ini ------------------------------------------------------------
        KB (F_STEER, "ChopperSteering", "Enable",                  EnableSteeringPatches),
        KF (F_STEER, "ChopperSteering", "TurnResponseScale",       TurnResponseScale, -30.0f, -0.5f),
        KF (F_STEER, "ChopperSteering", "TurnClamp",               TurnClamp, 0.4f, 5.0f),
        KF (F_STEER, "ChopperSteering", "HighSpeedTurnClampScale", HighSpeedTurnClampScale, 0.3f, 1.5f),
        KF (F_STEER, "ChopperSteering", "HighSpeedClampThreshold", HighSpeedClampThreshold, 20.0f, 200.0f),
        KF (F_STEER, "ChopperSteering", "LowSpeedTurnClampScale",  LowSpeedTurnClampScale, 0.3f, 1.5f),
        KF (F_STEER, "ChopperSteering", "LowSpeedClampThreshold",  LowSpeedClampThreshold, 2.0f, 50.0f),
        KF (F_STEER, "ChopperSteering", "AttackTurnClampScale",    AttackTurnClampScale, 0.3f, 1.5f),
        KF (F_STEER, "ChopperSteering", "SteeringSlewSeconds",     SteeringSlewSeconds, 0.05f, 3.0f),
        KB (F_STEER, "ChopperSmoothing", "Enable",                  EnableSmoothingPatches),
        KB (F_STEER, "ChopperSmoothing", "PatchOutputSmoothing",    PatchOutputSmoothing),
        KF (F_STEER, "ChopperSmoothing", "SmoothingOldWeight",      SmoothingOldWeight, 0.0f, 15.0f),
        KF (F_STEER, "ChopperSmoothing", "SmoothingFinalScale",     SmoothingFinalScale, 0.01f, 1.0f),
        KB (F_STEER, "ChopperSmoothing", "PatchDestVelFilter",      PatchDestVelFilter),
        KF (F_STEER, "ChopperSmoothing", "DestVelFilterOldWeight",  DestVelFilterOldWeight, 0.0f, 15.0f),
        KF (F_STEER, "ChopperSmoothing", "DestVelFilterFinalScale", DestVelFilterFinalScale, 0.01f, 1.0f),
        KB (F_STEER, "ChopperSmoothing", "AllowUnstableFilterGain", AllowUnstableFilterGain),
        KB (F_STEER, "ChopperSmoothing", "LogFilterGains",          LogFilterGains),
        // ---- Speed.ini ----------------------------------------------------------------------
        KB (F_SPEED, "ChopperSpeed", "Enable",               EnableSpeedRegulator),
        KF (F_SPEED, "ChopperSpeed", "SpeedFactor",          SpeedFactor, 0.5f, 2.5f),
        KF (F_SPEED, "ChopperSpeed", "MaxSpeed",             SpeedMax, 0.0f, 400.0f),
        KF (F_SPEED, "ChopperSpeed", "SpeedPush",            SpeedPush, 0.0f, 1.0f),
        KF (F_SPEED, "ChopperSpeed", "OverspeedBrake",       OverspeedBrake, 0.0f, 1.0f),
        KF (F_SPEED, "ChopperSpeed", "MinApplySpeed",        MinApplySpeed, 5.0f, 250.0f),
        KB (F_SPEED, "ChopperSpeed", "TurnSlowdown",         TurnSlowdown),
        KF (F_SPEED, "ChopperSpeed", "TurnSlowdownStrength", TurnSlowdownStrength, 0.0f, 500.0f),
        KB (F_SPEED, "ChopperSpeed", "SkipDuringSkid",       SkipDuringSkid),
        KF (F_SPEED, "ChopperSpeed", "MinChaseSpeed",        MinChaseSpeed, 0.0f, 200.0f),
        KF (F_SPEED, "ChopperSpeed", "MaxChaseSpeed",        MaxChaseSpeed, 0.0f, 400.0f),
        KI (F_SPEED, "ChopperSpeed", "VerboseIntervalMs",    VerboseIntervalMs, 100, 60000),
        KF (F_SPEED, "ChopperSpeed", "ArrivalSpeedScale",    ArrivalSpeedScale, 0.3f, 1.5f),
        KF (F_SPEED, "ChopperSpeed", "StoppedSpeedScale",    StoppedSpeedScale, 0.2f, 1.5f),
        KF (F_SPEED, "ChopperSpeed", "RecoverySpeedScale",   RecoverySpeedScale, 0.2f, 1.5f),
        KF (F_SPEED, "ChopperSpeed", "ReversalSpeedScale",   ReversalSpeedScale, 0.2f, 1.5f),
        // ---- Visibility.ini ----------------------------------------------------------------------
        KB (F_VIS, "ChopperVision", "Enable",          EnableVisionPatches),
        KB (F_VIS, "ChopperVision", "SeeThroughWalls", SeeThroughWalls),
        // ---- Predictions.ini ---------------------------------------------------------------------------
        KB (F_PRED, "Prediction", "Enable",              EnablePrediction),
        KB (F_PRED, "Prediction", "UsePlayerPositionSource", UsePlayerPositionSource),
        KF (F_PRED, "Prediction", "PredictionLeadScale", PredictionLeadScale, 0.0f, 2.0f),
        KF (F_PRED, "Prediction", "TurnLeadReduction",   TurnLeadReduction, 0.0f, 1.0f),
        KF (F_PRED, "Prediction", "BrakeLeadReduction",  BrakeLeadReduction, 0.0f, 1.0f),
        KF (F_PRED, "Prediction", "HighSpeedLeadBoost",  HighSpeedLeadBoost, 0.0f, 1.0f),
        KF (F_PRED, "Prediction", "HighSpeedThreshold",  HighSpeedThreshold, 10.0f, 200.0f),
        KF (F_PRED, "Prediction", "StablePathSeconds",   StablePathSeconds, 0.2f, 30.0f),
        KI (F_PRED, "Prediction", "MinValidSamples",     MinValidSamples, 3, 100),
        KF (F_PRED, "Prediction", "FreezeSeconds",       FreezeSeconds, 0.1f, 3.0f),
        KF (F_PRED, "Prediction", "TeleportJumpDistance", TeleportJumpDistance, 5.0f, 100.0f),
        KF (F_PRED, "Prediction", "MaxTargetAcceleration", MaxTargetAcceleration, 10.0f, 300.0f),
        KB (F_PRED, "Prediction", "LogEstimatorEvents",  LogEstimatorEvents),
        KB (F_PRED, "StationaryPlayer", "Enable",                    EnableStationaryPlayer),
        KF (F_PRED, "StationaryPlayer", "StoppedSpeedThreshold",     StoppedSpeedThreshold, 0.5f, 15.0f),
        KF (F_PRED, "StationaryPlayer", "StoppedDetectSeconds",      StoppedDetectSeconds, 0.2f, 10.0f),
        KF (F_PRED, "StationaryPlayer", "StoppedLeadScale",          StoppedLeadScale, 0.0f, 1.0f),
        KB (F_PRED, "StationaryPlayer", "SuppressAttacksWhenStopped", SuppressAttacksWhenStopped),
        KI (F_PRED, "StationaryPlayer", "MovingClearSamples",        MovingClearSamples, 1, 20),
        KB (F_PRED, "StateMachine", "Enable",              EnableStateMachine),
        KB (F_PRED, "StateMachine", "LogTransitions",      LogStateTransitions),
        KF (F_PRED, "StateMachine", "StateTimeoutSeconds", StateTimeoutSeconds, 5.0f, 300.0f),
        KF (F_PRED, "StateMachine", "TransitionLogCooldownSeconds", TransitionLogCooldownSeconds, 0.5f, 30.0f),
        // ---- Recovery.ini ---------------------------------------------------------------------------------
        KB (F_RECOV, "DirectionChangeRecovery", "Enable",              EnableDirectionChange),
        KF (F_RECOV, "DirectionChangeRecovery", "ReversalDot",         ReversalDot, -1.0f, 0.5f),
        KF (F_RECOV, "DirectionChangeRecovery", "SharpTurnDegPerSec",  SharpTurnDegPerSec, 20.0f, 720.0f),
        KF (F_RECOV, "DirectionChangeRecovery", "RecoverySeconds",     DirectionRecoverySeconds, 0.2f, 10.0f),
        KF (F_RECOV, "DirectionChangeRecovery", "ReversalLeadReduction", ReversalLeadReduction, 0.0f, 1.0f),
        KB (F_RECOV, "DirectionChangeRecovery", "AbortAttackOnReversal", AbortAttackOnReversal),
        KF (F_RECOV, "DirectionChangeRecovery", "CooldownSeconds",     DirectionCooldownSeconds, 0.0f, 30.0f),
        KB (F_RECOV, "StuckRecovery", "Enable",             EnableStuckRecovery),
        KF (F_RECOV, "StuckRecovery", "MinProgressSpeed",   StuckMinProgressSpeed, 0.5f, 30.0f),
        KF (F_RECOV, "StuckRecovery", "NoProgressSeconds",  StuckNoProgressSeconds, 1.0f, 30.0f),
        KF (F_RECOV, "StuckRecovery", "RecoverySeconds",    StuckRecoverySeconds, 0.5f, 30.0f),
        KF (F_RECOV, "StuckRecovery", "RecoveryLeadScale",  StuckRecoveryLeadScale, 0.1f, 1.0f),
        KF (F_RECOV, "StuckRecovery", "RecoveryExtraHeight", StuckRecoveryExtraHeight, 0.0f, 20.0f),
        KF (F_RECOV, "StuckRecovery", "RecoveryCooldownSeconds", StuckRecoveryCooldownSeconds, 1.0f, 120.0f),
        KB (F_RECOV, "AntiCircling", "Enable",                EnableAntiCircling),
        KF (F_RECOV, "AntiCircling", "CirclingTurnDegPerSec", CirclingTurnDegPerSec, 20.0f, 360.0f),
        KF (F_RECOV, "AntiCircling", "CirclingDetectSeconds", CirclingDetectSeconds, 0.5f, 30.0f),
        KF (F_RECOV, "AntiCircling", "OscillationFlipsPerSec", OscillationFlipsPerSec, 1.0f, 20.0f),
        KF (F_RECOV, "AntiCircling", "RecoverySeconds",       CirclingRecoverySeconds, 0.5f, 30.0f),
        KF (F_RECOV, "AntiCircling", "CooldownSeconds",       CirclingCooldownSeconds, 0.0f, 60.0f),
        // ---- Aggression.ini ------------------------------------------------------------------------------------
        KB (F_AGGR, "Aggression", "Enable",                    EnableAggression),
        KF (F_AGGR, "Aggression", "AttackFrequency",           AttackFrequency, 0.0f, 2.0f),
        KF (F_AGGR, "Aggression", "AttackWillingness",         AttackWillingness, 0.0f, 2.0f),
        KF (F_AGGR, "Aggression", "AlignmentTolerance",        AlignmentTolerance, -0.5f, 0.3f),
        KF (F_AGGR, "Aggression", "CatchUpUrgency",            CatchUpUrgency, 0.0f, 1.0f),
        KF (F_AGGR, "Aggression", "CatchUpStartDistance",      CatchUpStartDistance, 30.0f, 500.0f),
        KF (F_AGGR, "Aggression", "CatchUpMaxSpeedBoost",      CatchUpMaxSpeedBoost, 0.0f, 1.0f),
        KF (F_AGGR, "Aggression", "FailedAttackBackoffSeconds", FailedAttackBackoffSeconds, 0.0f, 60.0f),
        KF (F_AGGR, "Aggression", "ArrivalGraceSeconds",       ArrivalGraceSeconds, 0.0f, 60.0f),
        KB (F_AGGR, "DynamicAggression", "Enable",               EnableDynamicAggression),
        KF (F_AGGR, "DynamicAggression", "MinLevel",             DynAggrMin, 0.0f, 1.0f),
        KF (F_AGGR, "DynamicAggression", "MaxLevel",             DynAggrMax, 0.0f, 1.0f),
        KF (F_AGGR, "DynamicAggression", "TimeToMaxMinutes",     DynAggrTimeToMaxMinutes, 0.1f, 60.0f),
        KF (F_AGGR, "DynamicAggression", "FailedAttackIncrease", DynAggrFailedAttackIncrease, 0.0f, 1.0f),
        KB (F_AGGR, "DynamicAggression", "ResetOnEngagement",    DynAggrResetOnEngagement),
        KF (F_AGGR, "DynamicAggression", "DecayPerMinute",       DynAggrDecayPerMinute, 0.0f, 2.0f),
        // ---- AIHelicopterBehavior.ini -----------------------------------------------------------------------------
        KB (F_BEHAV, "FallbackBehavior", "Enable",             EnableFallback),
        KF (F_BEHAV, "FallbackBehavior", "FallbackSeconds",    FallbackSeconds, 1.0f, 60.0f),
        KI (F_BEHAV, "FallbackBehavior", "MaxFaultsPerMinute", FallbackMaxFaultsPerMinute, 1, 1000),
        KI (F_BEHAV, "FallbackBehavior", "PermanentStandDownFaults", PermanentStandDownFaults, 20, 100000),
        KB (F_BEHAV, "ExitBehavior", "DisableFuelBasedExit",      DisableFuelBasedExit),
        KB (F_BEHAV, "ExitBehavior", "Enable",                    EnableExitActionPatches),
        KB (F_BEHAV, "ExitBehavior", "PatchExitFlySpeed",         PatchExitFlySpeed),
        KB (F_BEHAV, "ExitBehavior", "PatchExitSeekUpThreshold",  PatchExitSeekUpThreshold),
        KB (F_BEHAV, "ExitBehavior", "PatchExitSeekAheadDistance", PatchExitSeekAheadDistance),
        KB (F_BEHAV, "ExitBehavior", "PatchExitSeekCarHeight",    PatchExitSeekCarHeight),
        KB (F_BEHAV, "ExitBehavior", "PatchExitReachDistance",    PatchExitReachDistance),
        KB (F_BEHAV, "ExitBehavior", "PatchExitFlyout",           PatchExitFlyout),
        KB (F_BEHAV, "ExitBehavior", "PatchExitFinishRules",      PatchExitFinishRules),
        KB (F_BEHAV, "ExitBehavior", "PatchExitDoDrivingMode",    PatchExitDoDrivingMode),
        KF (F_BEHAV, "ExitBehavior", "ExitFlySpeed",              ExitFlySpeed, 40.0f, 220.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitSeekUpThreshold",       ExitSeekUpThreshold, 0.0f, 80.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitSeekAheadDistance",     ExitSeekAheadDistance, 0.0f, 150.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitSeekCarReachDistanceSq", ExitSeekCarReachDistanceSq, 1.0f, 2500.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitRightScale",            ExitRightScale, -30.0f, 30.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitFlyoutBackScale",       ExitFlyoutBackScale, -300.0f, 0.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitFlyoutExtraHeight",     ExitFlyoutExtraHeight, -15.0f, 30.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitTargetHeight",          ExitTargetHeight, 0.0f, 500.0f),
        KF (F_BEHAV, "ExitBehavior", "ExitFinishDistanceSq",      ExitFinishDistanceSq, 25.0f, 1000000.0f),
        KU8(F_BEHAV, "ExitBehavior", "ExitDoDrivingMode",         ExitDoDrivingMode, 0, 15),
        // ---- Navigation.ini -----------------------------------------------------------------------------------------
        KB (F_NAV, "HeliSheet", "EnableSafeOverride",       EnableSheetSafeOverride),
        KF (F_NAV, "HeliSheet", "MaxHeightAboveTarget",     SheetMaxHeightAboveTarget, 8.0f, 200.0f),
        KF (F_NAV, "HeliSheet", "OverrideMaxHorizDistance", SheetOverrideMaxHorizDistance, 10.0f, 300.0f),
        KB (F_NAV, "HeliSheet", "DescendWhenPlayerStopped", SheetDescendWhenPlayerStopped),
        KB (F_NAV, "HeliSheet", "RespectHeliSheetDuringSkid", RespectHeliSheetDuringSkid),
        KB (F_NAV, "HeliSheet", "IgnoreHeliSheet",          IgnoreHeliSheet),

        // ---- Radio.ini -----------------------------------------------------
        KB (F_RADIO, "HeliRadioChat", "Enable",                EnableRadioChat),
        KB (F_RADIO, "HeliRadioChat", "AnnounceArrival",       RadioAnnounceArrival),
        KB (F_RADIO, "HeliRadioChat", "SpotterCalls",          RadioSpotterCalls),
        KB (F_RADIO, "HeliRadioChat", "LostVisualCalls",       RadioLostVisualCalls),
        KF (F_RADIO, "HeliRadioChat", "LostVisualDistance",    RadioLostVisualDistance, 60.0f, 500.0f),
        KF (F_RADIO, "HeliRadioChat", "LostVisualSeconds",     RadioLostVisualSeconds, 1.0f, 30.0f),
        KB (F_RADIO, "HeliRadioChat", "PositionCalls",         RadioPositionCalls),
        KF (F_RADIO, "HeliRadioChat", "StoppedCallSeconds",    RadioStoppedCallSeconds, 0.5f, 30.0f),
        KB (F_RADIO, "HeliRadioChat", "FuelCalls",             RadioFuelCalls),
        KF (F_RADIO, "HeliRadioChat", "FuelWarnSeconds",       RadioFuelWarnSeconds, 3.0f, 60.0f),
        KB (F_RADIO, "HeliRadioChat", "ArrestCalls",           RadioArrestCalls),
        KF (F_RADIO, "HeliRadioChat", "ArrestDistance",        RadioArrestDistance, 5.0f, 200.0f),
        KF (F_RADIO, "HeliRadioChat", "ArrestSeconds",         RadioArrestSeconds, 1.0f, 30.0f),
        KB (F_RADIO, "HeliRadioChat", "AnnounceAttacks",       RadioAnnounceAttacks),
        KB (F_RADIO, "HeliRadioChat", "HazardCalls",           RadioHazardCalls),
        KI (F_RADIO, "HeliRadioChat", "HazardContext",         RadioHazardContext, 0, 15),
        KB (F_RADIO, "HeliRadioChat", "SwarmCalls",            RadioSwarmCalls),
        KF (F_RADIO, "HeliRadioChat", "GlobalCooldownSeconds", RadioGlobalCooldownSeconds, 1.0f, 120.0f),
        KF (F_RADIO, "HeliRadioChat", "EventCooldownSeconds",  RadioEventCooldownSeconds, 5.0f, 600.0f),
        KI (F_RADIO, "HeliRadioChat", "MaxLinesPerMinute",     RadioMaxLinesPerMinute, 1, 30),
        KI (F_RADIO, "HeliRadioChat", "HeliSpeakerRole",       RadioHeliSpeakerRole, 0, 2),
        KB (F_RADIO, "HeliRadioChat", "LogRadioEvents",        RadioLogEvents),
        // ---- Debug.ini ------------------------------------------------------------------------------------------------
        KB (F_DEBUG, "Telemetry", "Enable",           EnableTelemetry),
        KI (F_DEBUG, "Telemetry", "IntervalMs",       TelemetryIntervalMs, 100, 60000),
        KB (F_DEBUG, "Telemetry", "IncludeAiState",   TelemetryAiState),
        KB (F_DEBUG, "Telemetry", "LogHeliLifecycle", LogHeliLifecycle),
        KB (F_DEBUG, "ChopperSpawner", "Enable",                          EnableDispatchPatches),
        KB (F_DEBUG, "ChopperSpawner", "AllowCopheliWeightedSelection",   AllowCopheliWeightedSelection),
        KB (F_DEBUG, "ChopperSpawner", "IgnoreExistingHeliForSelectorTrigger", IgnoreExistingHeliForSelectorTrigger),
        KB (F_DEBUG, "ChopperSpawner", "IgnoreExistingHeliForSpecialSpawn", IgnoreExistingHeliForSpecialSpawn),
        KB (F_DEBUG, "ChopperSpawner", "ForceWeightedSelectorAlwaysReturnCopheli", ForceWeightedSelectorAlwaysReturnCopheli),
        KB (F_DEBUG, "ChopperSpawner", "BypassSpawnCapForImmediateRequests", BypassSpawnCapForImmediateRequests),
        KB (F_DEBUG, "ChopperSpawner", "LogSpawnEvents",                  LogSpawnEvents),
        KB (F_DEBUG, "ChopperCollision", "EnableProximityTelemetry", EnableProximityTelemetry),
        KF (F_DEBUG, "ChopperCollision", "WarnDistance",             CollisionWarnDistance, 5.0f, 200.0f),
        KF (F_DEBUG, "ChopperCollision", "PredictSeconds",           CollisionPredictSeconds, 0.5f, 10.0f),
        KI (F_DEBUG, "ChopperCollision", "LogIntervalMs",            CollisionLogIntervalMs, 250, 60000),
        KI (F_DEBUG, "Diagnostics",      "AiUpdateIntervalMs",       AiUpdateIntervalMs, 0, 5000),
        };
        constexpr int kKeyCount = sizeof(kKeys) / sizeof(kKeys[0]);

        // Snapshot of compile-time defaults (for regeneration + diagnostics).
        struct DefaultVal { float f; int i; bool b; uint8_t u8; };
        DefaultVal gDefaults[kKeyCount];
        void CaptureDefaults() {
            for (int i = 0; i < kKeyCount; ++i) {
                switch (kKeys[i].type) {
                case T_BOOL:  gDefaults[i].b  = *static_cast<bool*>(kKeys[i].ptr); break;
                case T_FLOAT: gDefaults[i].f  = *static_cast<float*>(kKeys[i].ptr); break;
                case T_INT:   gDefaults[i].i  = *static_cast<int*>(kKeys[i].ptr); break;
                case T_U8:    gDefaults[i].u8 = *static_cast<uint8_t*>(kKeys[i].ptr); break;
                }
            }
        }

        // ---------------------------------------------------------------- helpers
        char gConfigDir[MAX_PATH] = "";

        void Trim(char* s) {
            char* p = s;
            while (*p == ' ' || *p == '\t') ++p;
            if (p != s) std::memmove(s, p, std::strlen(p) + 1);
            size_t n = std::strlen(s);
            while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n'))
                s[--n] = '\0';
        }

        bool IEq(const char* a, const char* b) { return _stricmp(a, b) == 0; }

        // Find the owning file of a known section (-1 if unknown).
        int SectionOwner(const char* section) {
            for (int i = 0; i < kKeyCount; ++i)
                if (IEq(kKeys[i].section, section)) return kKeys[i].file;
            return -1;
        }

        int FindKey(const char* section, const char* key) {
            for (int i = 0; i < kKeyCount; ++i)
                if (IEq(kKeys[i].section, section) && IEq(kKeys[i].key, key)) return i;
            return -1;
        }

        struct FileStats {
            int sections = 0, keys = 0, unknownKeys = 0, unknownSections = 0,
                duplicates = 0, invalid = 0, misplaced = 0, clamped = 0;
            bool present = false;
        };
        FileStats gStats[F_COUNT];

        bool ParseFloatStrict(const char* text, double* out) {
            char* end = nullptr;
            const double v = std::strtod(text, &end);
            if (end == text) return false;
            while (*end == ' ' || *end == '\t') ++end;
            if (*end != '\0') return false;
            if (!(v == v) || v > FLT_MAX || v < -FLT_MAX) return false;
            *out = v;
            return true;
        }

        bool ParseIntStrict(const char* text, long* out) {
            char* end = nullptr;
            const long v = std::strtol(text, &end, 10);
            if (end == text) return false;
            while (*end == ' ' || *end == '\t') ++end;
            if (*end != '\0') return false;
            *out = v;
            return true;
        }

        void ApplyKey(int fileId, int keyIdx, const char* value, FileStats& st) {
            const KeyDef& k = kKeys[keyIdx];
            const char* fname = kFileNames[fileId];
            switch (k.type) {
            case T_BOOL: {
                if (!std::strcmp(value, "0")) *static_cast<bool*>(k.ptr) = false;
                else if (!std::strcmp(value, "1")) *static_cast<bool*>(k.ptr) = true;
                else {
                    Log::Warn("[Config] %s [%s] %s=%s rejected: expected 0 or 1; using %d.",
                              fname, k.section, k.key, value,
                              *static_cast<bool*>(k.ptr) ? 1 : 0);
                    st.invalid++;
                    return;
                }
                break;
            }
            case T_FLOAT: {
                double v;
                if (!ParseFloatStrict(value, &v)) {
                    Log::Warn("[Config] %s [%s] %s=%s rejected: expected float; using %.6g.",
                              fname, k.section, k.key, value, *static_cast<float*>(k.ptr));
                    st.invalid++;
                    return;
                }
                float f = static_cast<float>(v);
                if (f < k.mn || f > k.mx) {
                    const float c = f < k.mn ? k.mn : k.mx;
                    Log::Warn("[Config] %s [%s] %s=%.6g out of range [%.6g..%.6g]; clamped to %.6g.",
                              fname, k.section, k.key, f, k.mn, k.mx, c);
                    f = c;
                    st.clamped++;
                }
                *static_cast<float*>(k.ptr) = f;
                break;
            }
            case T_INT: case T_U8: {
                long v;
                if (!ParseIntStrict(value, &v)) {
                    Log::Warn("[Config] %s [%s] %s=%s rejected: expected integer.",
                              fname, k.section, k.key, value);
                    st.invalid++;
                    return;
                }
                if (v < static_cast<long>(k.mn) || v > static_cast<long>(k.mx)) {
                    const long c = v < static_cast<long>(k.mn)
                                 ? static_cast<long>(k.mn) : static_cast<long>(k.mx);
                    Log::Warn("[Config] %s [%s] %s=%ld out of range [%ld..%ld]; clamped to %ld.",
                              fname, k.section, k.key, v,
                              static_cast<long>(k.mn), static_cast<long>(k.mx), c);
                    v = c;
                    st.clamped++;
                }
                if (k.type == T_INT) *static_cast<int*>(k.ptr) = static_cast<int>(v);
                else *static_cast<uint8_t*>(k.ptr) = static_cast<uint8_t>(v);
                break;
            }
            }
            st.keys++;
            if (gCfg.VerboseConfigLogging)
                Log::Info("[Config] %s [%s] %s=%s applied.", fname, k.section, k.key, value);
        }

        void ParseFile(int fileId) {
            FileStats& st = gStats[fileId];
            char path[MAX_PATH];
            std::snprintf(path, MAX_PATH, "%s%s", gConfigDir, kFileNames[fileId]);

            FILE* f = std::fopen(path, "rb");
            if (!f) return;   // caller handles missing files
            st.present = true;

            std::fseek(f, 0, SEEK_END);
            long size = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (size < 0 || size > 4 * 1024 * 1024) { std::fclose(f); return; }
            char* buf = static_cast<char*>(std::malloc(size + 1));
            if (!buf) { std::fclose(f); return; }
            size_t rd = std::fread(buf, 1, size, f);
            std::fclose(f);
            buf[rd] = '\0';

            char* p = buf;
            if (rd >= 3 && static_cast<unsigned char>(p[0]) == 0xEF
                && static_cast<unsigned char>(p[1]) == 0xBB
                && static_cast<unsigned char>(p[2]) == 0xBF)
                p += 3;   // UTF-8 BOM

            static bool seen[kKeyCount];
            std::memset(seen, 0, sizeof(seen));

            char section[96] = "";
            int  sectionOwner = -2;          // -2 none yet, -1 unknown, >=0 file id
            bool sectionCounted = false;

            while (*p) {
                char* lineEnd = std::strchr(p, '\n');
                char line[512];
                size_t len = lineEnd ? static_cast<size_t>(lineEnd - p)
                                     : std::strlen(p);
                if (len >= sizeof(line)) len = sizeof(line) - 1;
                std::memcpy(line, p, len);
                line[len] = '\0';
                p = lineEnd ? lineEnd + 1 : p + std::strlen(p);

                Trim(line);
                if (!line[0] || line[0] == ';' || line[0] == '#') continue;

                if (line[0] == '[') {
                    char* close = std::strchr(line, ']');
                    if (!close) {
                        Log::Warn("[Config] %s: malformed section header \"%s\".",
                                  kFileNames[fileId], line);
                        st.invalid++;
                        section[0] = '\0'; sectionOwner = -2;
                        continue;
                    }
                    *close = '\0';
                    std::strncpy(section, line + 1, sizeof(section) - 1);
                    section[sizeof(section) - 1] = '\0';
                    Trim(section);
                    sectionOwner = SectionOwner(section);
                    sectionCounted = false;
                    if (sectionOwner == -1) {
                        st.unknownSections++;
                        if (gCfg.UnknownKeyWarnings)
                            Log::Warn("[Config] %s: unknown section [%s] ignored.",
                                      kFileNames[fileId], section);
                    } else if (sectionOwner != fileId) {
                        st.misplaced++;
                        Log::Error("[Config] %s: section [%s] belongs in %s - "
                                   "misplaced section IGNORED.",
                                   kFileNames[fileId], section,
                                   kFileNames[sectionOwner]);
                    }
                    continue;
                }

                char* eq = std::strchr(line, '=');
                if (!eq) {
                    Log::Warn("[Config] %s [%s]: malformed line \"%s\".",
                              kFileNames[fileId], section, line);
                    st.invalid++;
                    continue;
                }
                *eq = '\0';
                char key[96], value[256];
                std::strncpy(key, line, sizeof(key) - 1); key[sizeof(key) - 1] = '\0';
                std::strncpy(value, eq + 1, sizeof(value) - 1); value[sizeof(value) - 1] = '\0';
                Trim(key); Trim(value);

                if (sectionOwner == -2) continue;            // key before any section
                if (sectionOwner == -1) continue;            // unknown section
                if (sectionOwner != fileId) continue;        // misplaced section

                const int idx = FindKey(section, key);
                if (idx < 0) {
                    st.unknownKeys++;
                    if (gCfg.UnknownKeyWarnings)
                        Log::Warn("[Config] %s [%s]: unknown key \"%s\" ignored.",
                                  kFileNames[fileId], section, key);
                    continue;
                }
                if (seen[idx]) {
                    st.duplicates++;
                    Log::Warn("[Config] %s [%s]: duplicate key \"%s\" - first value kept.",
                              kFileNames[fileId], section, key);
                    continue;
                }
                seen[idx] = true;
                if (!sectionCounted) { st.sections++; sectionCounted = true; }
                ApplyKey(fileId, idx, value, st);
            }
            std::free(buf);
        }

        void WriteDefaultFile(int fileId) {
            char path[MAX_PATH];
            std::snprintf(path, MAX_PATH, "%s%s", gConfigDir, kFileNames[fileId]);
            FILE* f = std::fopen(path, "wb");
            if (!f) {
                Log::Warn("[Config] could not create missing %s.", kFileNames[fileId]);
                return;
            }
            std::fprintf(f,
                "; %s - regenerated with default values by HelicopterOptions.\r\n"
                "; Full documentation of every key:\r\n"
                ";   scripts\\HelicopterOptions\\Documentation\\CONFIGURATION_GUIDE.md\r\n\r\n",
                kFileNames[fileId]);
            const char* cur = "";
            for (int i = 0; i < kKeyCount; ++i) {
                if (kKeys[i].file != fileId) continue;
                if (!IEq(cur, kKeys[i].section)) {
                    cur = kKeys[i].section;
                    std::fprintf(f, "\r\n[%s]\r\n", cur);
                }
                switch (kKeys[i].type) {
                case T_BOOL:  std::fprintf(f, "%s=%d\r\n", kKeys[i].key, gDefaults[i].b ? 1 : 0); break;
                case T_FLOAT: std::fprintf(f, "%s=%g\r\n", kKeys[i].key, gDefaults[i].f); break;
                case T_INT:   std::fprintf(f, "%s=%d\r\n", kKeys[i].key, gDefaults[i].i); break;
                case T_U8:    std::fprintf(f, "%s=%u\r\n", kKeys[i].key, gDefaults[i].u8); break;
                }
            }
            std::fclose(f);
            Log::Info("[Config] %s was missing - regenerated with defaults.", kFileNames[fileId]);
        }

    } // namespace

    bool LoadConfig(void* moduleHandle) {
        CaptureDefaults();

        char moduleDir[MAX_PATH]{};
        DWORD n = GetModuleFileNameA(reinterpret_cast<HMODULE>(moduleHandle), moduleDir, MAX_PATH);
        if (n == 0 || n >= MAX_PATH) {
            Log::Error("[Config] cannot resolve module path; using built-in defaults.");
            Validation::Sanitize();
            SyncLiveFloats();
            return false;
        }
        char* slash = std::strrchr(moduleDir, '\\');
        if (slash) *(slash + 1) = '\0';

        // Legacy monolithic INI: detect, warn, IGNORE (never merged).
        {
            char legacy[MAX_PATH];
            std::snprintf(legacy, MAX_PATH, "%sHelicopterOptions.ini", moduleDir);
            if (GetFileAttributesA(legacy) != INVALID_FILE_ATTRIBUTES) {
                Log::Warn("[Config] Legacy scripts\\HelicopterOptions.ini detected but ignored.");
                Log::Warn("[Config] Settings now belong in scripts\\HelicopterOptions\\Configuration\\");
            }
        }

        std::snprintf(gConfigDir, MAX_PATH, "%sHelicopterOptions\\Configuration\\", moduleDir);
        Log::Info("[Config] Root: %s", gConfigDir);

        // Ensure the directory chain exists (harmless if already present).
        {
            char d1[MAX_PATH];
            std::snprintf(d1, MAX_PATH, "%sHelicopterOptions", moduleDir);
            CreateDirectoryA(d1, nullptr);
            std::snprintf(d1, MAX_PATH, "%sHelicopterOptions\\Configuration", moduleDir);
            CreateDirectoryA(d1, nullptr);
        }

        const DWORD dirAttrs = GetFileAttributesA(gConfigDir);
        const bool dirOk = dirAttrs != INVALID_FILE_ATTRIBUTES
                        && (dirAttrs & FILE_ATTRIBUTE_DIRECTORY);
        if (!dirOk)
            Log::Error("[Config] configuration directory unreadable - "
                       "using built-in defaults for everything.");

        int loaded = 0;
        for (int fid = 0; fid < F_COUNT && dirOk; ++fid) {
            char path[MAX_PATH];
            std::snprintf(path, MAX_PATH, "%s%s", gConfigDir, kFileNames[fid]);
            const bool exists = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
            if (!exists) {
                Log::Warn("[Config] %s missing - defaults in effect for its sections.",
                          kFileNames[fid]);
                if (gCfg.AutoCreateMissingFiles) WriteDefaultFile(fid);
                continue;
            }
            Log::Verbose("[Config] Loading %s", kFileNames[fid]);
            ParseFile(fid);
            if (gStats[fid].present) ++loaded;
        }

        // Summary ------------------------------------------------------------
        int totalSections = 0, totalKeys = 0, totalUnknown = 0, totalDup = 0,
            totalInvalid = 0, totalMisplaced = 0, totalClamped = 0;
        for (int fid = 0; fid < F_COUNT; ++fid) {
            const FileStats& st = gStats[fid];
            totalSections += st.sections;
            totalKeys += st.keys;
            totalUnknown += st.unknownKeys + st.unknownSections;
            totalDup += st.duplicates;
            totalInvalid += st.invalid;
            totalMisplaced += st.misplaced;
            totalClamped += st.clamped;
            if (gCfg.StartupSummary && st.present)
                Log::Info("[Config] %s: %d section(s), %d key(s)%s",
                          kFileNames[fid], st.sections, st.keys,
                          (st.unknownKeys + st.duplicates + st.invalid + st.misplaced)
                              ? " (with warnings above)" : "");
        }
        Log::Info("[Config] Loaded %d/%d configuration files", loaded, F_COUNT);
        Log::Info("[Config] Parsed %d sections and %d settings", totalSections, totalKeys);
        Log::Info("[Config] Unknown keys/sections: %d  Duplicates: %d  Invalid: %d  "
                  "Clamped: %d  Misplaced sections: %d",
                  totalUnknown, totalDup, totalInvalid, totalClamped, totalMisplaced);

        if (gCfg.ConfigVersion != 2)
            Log::Warn("[Config] ConfigVersion=%d does not match this build's version 2 - "
                      "review CONFIGURATION_GUIDE.md for changes.", gCfg.ConfigVersion);

        Log::SetToFile(gCfg.LogToFile);
        Log::SetVerbose(gCfg.VerboseLogging);
        Validation::Sanitize();
        SyncLiveFloats();
        Log::Info("[Config] Validation passed");
        return loaded > 0;
    }

} // namespace Ini

void SyncLiveFloats() {
    gLive.ChopperVelDtGate        = kFrameMinPhysicsDelta;
    gLive.SkidCooldownThreshold   = gCfg.SkidCooldownThreshold;
    gLive.SkidEntryMaxDistance    = gCfg.SkidEntryMaxDistance;
    gLive.SkidEntryMinDistance    = gCfg.SkidEntryMinDistance;
    gLive.SkidEntryAlignmentDot   = gCfg.SkidEntryAlignmentDot;
    gLive.SkidEntryMaxHeightDelta = gCfg.SkidEntryMaxHeightDelta;
    gLive.LeadSpeedScale          = gCfg.LeadSpeedScale;
    gLive.LeadBase                = gCfg.LeadBase;
    gLive.LeadMax                 = gCfg.LeadMax;
    gLive.LeadSkidMultiplier      = gCfg.LeadSkidMultiplier;
    gLive.ChaseHeightSkid         = gCfg.ChaseHeightSkid;
    gLive.ChaseHeightClose        = gCfg.ChaseHeightClose;
    gLive.ChaseHeightHigh         = gCfg.ChaseHeightHigh;
    gLive.SideOffsetPos           = gCfg.SkidSideOffset;
    gLive.SideOffsetNeg           = -gCfg.SkidSideOffset;
    gLive.ApproachVelocityLead    = gCfg.SkidApproachVelocityLead;
    gLive.ApproachHeight          = gCfg.SkidApproachHeight;
    gLive.LowExtraHeight          = gCfg.SkidLowExtraHeight;
    gLive.StrikeStartDistance     = gCfg.SkidStrikeStartDistance;
    gLive.StrikeLateralTrigger    = gCfg.SkidStrikeLateralTriggerDistance;
    gLive.StrikeVelocityLead      = gCfg.SkidStrikeVelocityLead;
    gLive.StrikeBackScale         = gCfg.SkidStrikeBackScale;
    gLive.AbortAheadSq            = gCfg.SkidAbortDistanceAheadSq;
    gLive.AbortBehindSq           = gCfg.SkidAbortDistanceBehindSq;
    gLive.DestVelFilterOldWeight  = gCfg.DestVelFilterOldWeight;
    gLive.DestVelFilterFinalScale = gCfg.DestVelFilterFinalScale;
    gLive.AccelBudgetSpeedScale   = gCfg.AccelBudgetSpeedScale;
    gLive.TurnResponseScale       = gCfg.TurnResponseScale;
    gLive.TurnClampPos            = gCfg.TurnClamp;
    gLive.TurnClampNeg            = -gCfg.TurnClamp;
    gLive.SmoothingOldWeight      = gCfg.SmoothingOldWeight;
    gLive.SmoothingFinalScale     = gCfg.SmoothingFinalScale;
    gLive.ExitSeekUpThreshold     = gCfg.ExitSeekUpThreshold;
    gLive.ExitSeekAheadDistance   = gCfg.ExitSeekAheadDistance;
    gLive.ExitSeekCarReachDistanceSq = gCfg.ExitSeekCarReachDistanceSq;
    gLive.ExitRightScale          = gCfg.ExitRightScale;
    gLive.ExitFlyoutBackScale     = gCfg.ExitFlyoutBackScale;
    gLive.ExitFlyoutExtraHeight   = gCfg.ExitFlyoutExtraHeight;
    gLive.ExitTargetHeight        = gCfg.ExitTargetHeight;
    gLive.ExitFinishDistanceSq    = gCfg.ExitFinishDistanceSq;
}
