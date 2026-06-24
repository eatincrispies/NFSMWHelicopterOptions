// HelicopterOptions Audited Core Build - NFSMW 2005 cop helicopter tuning
// -----------------------------------------------------------------------------
// Release-safety goal:
//   * Do NOT embed EA decompiled source into the ASI.
//   * Patch only byte-guarded sites inside the existing game functions.
//   * Keep risky research layers disabled by default.
//   * Use logging so every applied/skipped/failed patch can be checked.
//
// This file is original mod/patch code. The uploaded heli sources are used only
// as a behavior map to identify which existing PC speed.exe constants/globals are
// being patched.
// -----------------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <ctime>
#include <cmath>

namespace HelicopterOptions {

    static constexpr uintptr_t kExpectedImageBase = 0x00400000u;
    static constexpr DWORD     kExpectedTimeDateStamp = 0x438E4C8Cu;

    static HMODULE gModule = nullptr;
    static FILE* gLogFile = nullptr;
    static char gLogPath[MAX_PATH]{};
    static bool gLogToFile = true;
    static bool gVerboseLogging = true;
    static int gPatchAppliedCount = 0;
    static int gPatchSkippedCount = 0;
    static int gPatchFailedCount = 0;

    struct Config {
        bool EnableSkidLogicPatches = false;
        bool ForceSkidHitAttribute = false;
        bool EnableSkidEntryGatePatches = false;
        bool EnableSkidStrikePatches = false;
        bool EnableLeadAndHeightPatches = false;
        bool EnableAIVehicleVelocityPatches = false;
        bool EnableAIVehicleOnDrivingDataPatches = false;
        bool EnableAIVehicleOnDrivingTurnPatches = false;
        bool EnableAIVehicleOnDrivingSmoothingPatches = false;
        bool ForceOnDrivingMaxSpeedCap = false;

        // AIActionHeliExit / AIVehicleHelicopter::UpdateFuel layer.
        // DisableFuelBasedHeliExit is the most important anti-lazy patch: it stops fuel reaching zero
        // from forcing AIGoalHeliExit. The action patches below only matter if some other system
        // still forces the exit goal.
        bool DisableFuelBasedHeliExit = false;
        bool EnableHeliExitActionPatches = false;
        bool PatchExitFlySpeed = false;
        bool PatchExitSeekUpThreshold = false;
        bool PatchExitSeekAheadDistance = false;
        bool PatchExitSeekCarReachDistance = false;
        bool PatchExitFlyoutBackScale = false;
        bool PatchExitFlyoutExtraHeight = false;
        bool PatchExitFinishRules = false;
        bool PatchExitDoDrivingMode = false;

        bool EnableHeliSheetPatches = false;
        bool ForceIgnoreHeliSheetInAllPursuitModes = false;
        bool RespectHeliSheetDuringSkid = false;

        float SkidCooldownThreshold = -1.0f;
        float SkidEntryMaxDistance = 55.0f;
        float SkidEntryMinDistance = 0.0f;
        float SkidEntryDotRequirement = 0.35f;
        float SkidEntryMaxHeightDelta = 20.0f;

        float StraightLeadSpeedScale = 0.45f;
        float StraightLeadBase = 35.0f;
        float StraightLeadMax = 60.0f;
        float StraightLeadSkidMultiplier = 0.70f;
        float StraightHeightPositiveTimer = 1.5f;
        float StraightHeightClose = 4.5f;
        float StraightHeightHigh = 8.0f;

        float SkidSideOffset = 4.25f;
        float SkidApproachVelocityLead = 0.30f;
        float SkidApproachHeight = 1.2f;
        float SkidLowExtraHeight = 3.0f;
        float SkidStrikeStartDistance = 8.0f;
        float SkidStrikeRightDotRequirement = 0.75f;
        float SkidStrikeVelocityLead = 0.14f;
        float SkidStrikeBackScale = -0.35f;
        float SkidAbortDistanceAheadSq = 3600.0f;
        float SkidAbortDistanceBehindSq = 400.0f;

        float DestinationVelocityMultiplier = 5.5f;
        float DestinationVelocityBlend = 0.25f;

        // AIVehicleHelicopter::OnDriving layer. These are the SimpleChopper movement/turning clamps.
        float MaxChopperAccel = 125.0f;
        float MinChopperAccel = 45.0f;
        float ChopperSpeedLimitRatio = 2.6f;
        float OnDrivingDecelSpeedScale = 0.85f;
        float OnDrivingTurnResponseScale = -12.0f;
        float OnDrivingTurnClamp = 2.0f;
        float OnDrivingSmoothingOldWeight = 3.0f;
        float OnDrivingSmoothingFinalScale = 0.25f;

        // Stock PC/source-ish exit values:
        // fuel exit branch active, fly speed 100, seek-up gate 15, seek-ahead 85, reach distance sq 25,
        // side/back flyout -9/-200, extra height 5, finish distance sq 22500.
        float ExitFlySpeed = 130.0f;
        float ExitSeekUpThreshold = 8.0f;
        float ExitSeekAheadDistance = 45.0f;
        float ExitSeekCarReachDistanceSq = 100.0f;
        float ExitRightNegativeScale = -6.0f;
        float ExitFlyoutBackScale = -65.0f;
        float ExitFlyoutExtraHeight = 0.0f;
        float ExitFinishHeight = 25.0f;
        float ExitFinishDistanceSq = 22500.0f;
        uint8_t ExitDoDrivingMode = 7;

        bool EnableLegacyRenderDistancePatch = false;
        float LegacyRenderDistanceThreshold = -1.0f;

        // HeliRenderConn / Pkt_Heli_Service layer. This does not steer the helicopter directly.
        // It controls the render connection feedback packet that reports whether the heli was drawn
        // and how far it was from the active camera. These are experimental visibility/service hints.
        bool EnableHeliRenderConnPatches = false;
        bool ForceHeliRenderServiceInView = false;
        bool ForceHeliRenderServiceDistance = false;
        bool PreventHeliRenderHideOnServiceFail = false;
        bool PatchHeliRenderResetDistance = false;
        float HeliRenderServiceDistance = 30.0f;
        float HeliRenderResetDistance = 999999.0f;

        // Extra Ghidra snippets supplied after the render build. These are OFF by default.
        // ForceAIGoalHeliRoadBlockSelection bypasses the car-type check in the roadblock goal selector
        // and always falls through to the AIGoalHeliRoadBlock string path. Use only for experiments.
        bool EnableGhidraExtraPatches = false;
        bool ForceAIGoalHeliRoadBlockSelection = false;

        // Dispatch/spawn layer found through copheli / CHOPPER / ReqHeliJoin xrefs.
        // These are OFF by default. EnableDispatchSpawnPatches=1 only enables the group;
        // each sub-patch is separately controlled to make crash testing easier.
        bool EnableDispatchSpawnPatches = false;
        bool AllowCopheliWeightedSelection = true;
        bool IgnoreExistingHeliForSelectorTrigger = false;
        bool IgnoreExistingHeliForSpecialSpawn = false;
        bool ForceWeightedSelectorAlwaysReturnCopheli = false;
        bool BypassSpawnCapForImmediateRequests = false;

        // HeliWash appears to be FX/physics initialization for the rotor wash object. It is not steering AI,
        // but changing the default Physics scalar may affect wash visuals/interaction if the object is active.
        bool EnableHeliWashPatches = false;
        float HeliWashPhysicsStrength = 1.0f;


        // Chopper::Speed - experimental speed/velocity regulator ported from the HeliRefined research build.
        // This is the real speed-cap/uncap layer: it hooks AIVehicleHelicopter::OnDriving to capture the live heli object,
        // reads heli+0x84 (mDriveSpeed), then nudges the rigidbody horizontal velocity toward a capped target.
        // Keep optional because it writes live physics velocity every tick.
        bool EnableChopperSpeed = false;
        float ChopperSpeedFactor = 1.15f;
        float ChopperMaxSpeed = 155.0f;          // 0.0 = no custom ceiling.
        float ChopperSpeedPush = 0.055f;         // How strongly speed is pushed upward toward target.
        float ChopperOverspeedBrake = 0.12f;     // How strongly speed is reduced when above target.
        float ChopperMinApplySpeed = 20.0f;
        bool ChopperTurnSlowdown = true;
        float ChopperTurnSlowdownStrength = 120.0f;
        bool ChopperIgnoreNegativeDesiredSpeed = true;

        // Chopper::Vision - IsPerpInSight branch patch. This keeps the heli from dropping into search/blue mode
        // when the player breaks line-of-sight. Optional because it effectively grants the heli wall vision.
        bool EnableVisionPatches = false;
        bool HeliSeesThroughWalls = false;

        // Logging/release-check layer. The DLL writes HelicopterOptions.log next to the ASI/DLL.
        // The log records detected EXE info, INI path, final clamped config, and every applied/skipped patch.
        bool LogToFile = true;
        bool VerboseLogging = true;

        bool EnableHeatBasedProfiles = false;
        bool EnableRaceBasedProfiles = false;
        bool InstallHeatObserverHook = false;
        bool LogHeatProfileChanges = false;
        uint32_t HeatProfileStartupHeat = 1;
        bool HeatProfileStartupIsRace = false;
    };

    struct HeatProfile {
        bool enabled = true;

        // AIActionHeliPursuit entry/straight/skid logic. Every runtime float listed in the INI is stored here.
        float SkidCooldownThreshold = -1.0f;
        float SkidEntryMaxDistance = 55.0f;
        float SkidEntryMinDistance = 0.0f;
        float SkidEntryDotRequirement = 0.35f;
        float SkidEntryMaxHeightDelta = 20.0f;

        float StraightLeadSpeedScale = 0.45f;
        float StraightLeadBase = 35.0f;
        float StraightLeadMax = 60.0f;
        float StraightLeadSkidMultiplier = 0.70f;
        float StraightHeightPositiveTimer = 1.5f;
        float StraightHeightClose = 4.5f;
        float StraightHeightHigh = 8.0f;

        float SkidSideOffset = 4.25f;
        float SkidApproachVelocityLead = 0.30f;
        float SkidApproachHeight = 1.2f;
        float SkidLowExtraHeight = 3.0f;
        float SkidStrikeStartDistance = 8.0f;
        float SkidStrikeRightDotRequirement = 0.75f;
        float SkidStrikeVelocityLead = 0.14f;
        float SkidStrikeBackScale = -0.35f;
        float SkidAbortDistanceAheadSq = 3600.0f;
        float SkidAbortDistanceBehindSq = 400.0f;

        // AIVehicleHelicopter / SimpleChopper movement layer.
        float DestinationVelocityMultiplier = 5.5f;
        float DestinationVelocityBlend = 0.25f;
        float MaxChopperAccel = 125.0f;
        float MinChopperAccel = 45.0f;
        float ChopperSpeedLimitRatio = 2.6f;
        float OnDrivingDecelSpeedScale = 0.85f;
        float OnDrivingTurnResponseScale = -12.0f;
        float OnDrivingTurnClamp = 2.0f;
        float OnDrivingSmoothingOldWeight = 3.0f;
        float OnDrivingSmoothingFinalScale = 0.25f;

        // AIActionHeliExit fallback values. These can also be heat/race swapped.
        float ExitFlySpeed = 130.0f;
        float ExitSeekUpThreshold = 8.0f;
        float ExitSeekAheadDistance = 45.0f;
        float ExitSeekCarReachDistanceSq = 100.0f;
        float ExitRightNegativeScale = -6.0f;
        float ExitFlyoutBackScale = -65.0f;
        float ExitFlyoutExtraHeight = 0.0f;
        float ExitFinishHeight = 25.0f;
        float ExitFinishDistanceSq = 22500.0f;
        uint8_t ExitDoDrivingMode = 7;
    };

    static HeatProfile gHeatProfiles[11];   // roam / heat01-heat10 profiles
    static HeatProfile gRaceProfiles[11];   // race / race01-race10 profiles, fallback from heat profiles
    static uint32_t gCurrentHeatLevel = 0;
    static bool gCurrentIsRacing = false;
    static uintptr_t gPlayerPerpVehicle = 0;

    static Config gCfg;

    static void LoadHeatProfiles(const char* path);
    static void ApplyHeatProfile(uint32_t heatLevel, const char* reason);
    static bool WriteMemory(void* dst, const void* src, size_t len);
    static uint32_t FloatBits(float value);

    // DLL-owned floats. x86 absolute-memory x87 instructions are redirected to these.
    static float fSkidCooldownThreshold;
    static float fSkidEntryMaxDistance;
    static float fSkidEntryMinDistance;
    static float fSkidEntryDotRequirement;
    static float fSkidEntryMaxHeightDelta;
    static float fStraightLeadSpeedScale;
    static float fStraightLeadBase;
    static float fStraightLeadMax;
    static float fStraightLeadSkidMultiplier;
    static float fStraightHeightPositiveTimer;
    static float fStraightHeightClose;
    static float fStraightHeightHigh;
    static float fSkidSideOffsetPositive;
    static float fSkidSideOffsetNegative;
    static float fSkidApproachVelocityLead;
    static float fSkidApproachHeight;
    static float fSkidLowExtraHeight;
    static float fSkidStrikeStartDistance;
    static float fSkidStrikeRightDotRequirement;
    static float fSkidStrikeVelocityLead;
    static float fSkidStrikeBackScale;
    static float fSkidAbortDistanceAheadSq;
    static float fSkidAbortDistanceBehindSq;
    static float fDestinationVelocityMultiplier;
    static float fDestinationVelocityBlend;
    static float fOnDrivingDecelSpeedScale;
    static float fOnDrivingTurnResponseScale;
    static float fOnDrivingTurnClampPositive;
    static float fOnDrivingTurnClampNegative;
    static float fOnDrivingSmoothingOldWeight;
    static float fOnDrivingSmoothingFinalScale;
    static float fExitFlySpeed;
    static float fExitSeekUpThreshold;
    static float fExitSeekAheadDistance;
    static float fExitSeekCarReachDistanceSq;
    static float fExitRightNegativeScale;
    static float fExitFlyoutBackScale;
    static float fExitFlyoutExtraHeight;
    static float fExitFinishHeight;
    static float fExitFinishDistanceSq;
    static float fLegacyRenderDistanceThreshold;
    static float fHeliRenderServiceDistance;
    static float fHeliRenderResetDistance;
    static float fHeliWashPhysicsStrength;


    // Chopper::Speed live hook state.
    static constexpr uintptr_t kOnDrivingHookVA = 0x00417A20u;
    static constexpr uintptr_t kGlobalHeliVehicleVA = 0x0090D8E4u;
    static constexpr unsigned kHeliOwnerOffset = 0x34u;
    static constexpr unsigned kHeliDriveSpeedOffset = 0x84u;
    static constexpr unsigned kOwnerGetRigidBodyVTableOffset = 0x54u;
    static constexpr unsigned kRigidBodyGetLinearVelocityVTableOffset = 0x24u;
    static const uint8_t kOnDrivingHookGuard[5] = { 0x83, 0xEC, 0x68, 0x53, 0x55 };

    static void* volatile gCapturedHeliThis = nullptr;
    static uint8_t* gOnDrivingTrampoline = nullptr;
    static bool gChopperSpeedHookInstalled = false;
    static bool gChopperSpeedThreadStarted = false;
    static DWORD gLastChopperSpeedLogTick = 0;

    struct FloatOperandPatch {
        const char* name;
        uintptr_t insnVA;
        uintptr_t operandVA;
        const uint8_t* expected;
        size_t expectedLen;
        const float* replacement;
    };

    static bool ResolveSidecarPath(char* out, DWORD outSize, const char* extension) {
        DWORD n = GetModuleFileNameA(gModule, out, outSize);
        if (n == 0 || n >= outSize) return false;
        char* slash = std::strrchr(out, '\\');
        char* dot = std::strrchr(out, '.');
        if (!dot || (slash && dot < slash)) dot = out + std::strlen(out);
        std::strncpy(dot, extension, outSize - static_cast<DWORD>(dot - out) - 1);
        out[outSize - 1] = '\0';
        return true;
    }

    static void OpenLogFile() {
        if (!gLogToFile) return;
        if (!ResolveSidecarPath(gLogPath, MAX_PATH, ".log")) return;
        gLogFile = std::fopen(gLogPath, "w");
        if (gLogFile) {
            std::time_t now = std::time(nullptr);
            std::fprintf(gLogFile, "HelicopterOptions Logging Build\n");
            std::fprintf(gLogFile, "Session start: %s", std::ctime(&now));
            std::fprintf(gLogFile, "Log path: %s\n\n", gLogPath);
            std::fflush(gLogFile);
        }
    }

    static void CloseLogFile() {
        if (gLogFile) {
            std::fflush(gLogFile);
            std::fclose(gLogFile);
            gLogFile = nullptr;
        }
    }

    static void Log(const char* fmt, ...) {
        char buf[1024]{};
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        OutputDebugStringA("[HelicopterOptions] ");
        OutputDebugStringA(buf);
        OutputDebugStringA("\n");

        if (gLogFile) {
            std::fprintf(gLogFile, "[HelicopterOptions] %s\n", buf);
            std::fflush(gLogFile);
        }
    }

    static bool ResolveIniPath(char* out, DWORD outSize) {
        return ResolveSidecarPath(out, outSize, ".ini");
    }

    static float Clamp(float v, float lo, float hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    static float ReadIniFloat(const char* path, const char* key, float fallback) {
        char buf[64]{};
        GetPrivateProfileStringA("HelicopterOptions", key, "", buf, sizeof(buf), path);
        if (buf[0] == '\0') return fallback;
        char* end = nullptr;
        const float v = static_cast<float>(std::strtod(buf, &end));
        return (end && end != buf) ? v : fallback;
    }

    static bool ReadIniBool(const char* path, const char* key, bool fallback) {
        return GetPrivateProfileIntA("HelicopterOptions", key, fallback ? 1 : 0, path) != 0;
    }

    static void ReadIniString(const char* path, const char* key, char* out, DWORD outSize, const char* fallback) {
        if (!out || outSize == 0) return;
        GetPrivateProfileStringA("HelicopterOptions", key, fallback ? fallback : "", out, outSize, path);
        out[outSize - 1] = '\0';
    }

    static float ReadIniFloatSection(const char* path, const char* section, const char* key, float fallback) {
        char buf[64]{};
        GetPrivateProfileStringA(section, key, "", buf, sizeof(buf), path);
        if (buf[0] == '\0') return fallback;
        char* end = nullptr;
        const float v = static_cast<float>(std::strtod(buf, &end));
        return (end && end != buf) ? v : fallback;
    }

    static bool ReadIniBoolSection(const char* path, const char* section, const char* key, bool fallback) {
        return GetPrivateProfileIntA(section, key, fallback ? 1 : 0, path) != 0;
    }

    static void FillHeatProfileFromCurrentConfig(HeatProfile& p) {
        p.enabled = true;

        p.SkidCooldownThreshold = gCfg.SkidCooldownThreshold;
        p.SkidEntryMaxDistance = gCfg.SkidEntryMaxDistance;
        p.SkidEntryMinDistance = gCfg.SkidEntryMinDistance;
        p.SkidEntryDotRequirement = gCfg.SkidEntryDotRequirement;
        p.SkidEntryMaxHeightDelta = gCfg.SkidEntryMaxHeightDelta;

        p.StraightLeadSpeedScale = gCfg.StraightLeadSpeedScale;
        p.StraightLeadBase = gCfg.StraightLeadBase;
        p.StraightLeadMax = gCfg.StraightLeadMax;
        p.StraightLeadSkidMultiplier = gCfg.StraightLeadSkidMultiplier;
        p.StraightHeightPositiveTimer = gCfg.StraightHeightPositiveTimer;
        p.StraightHeightClose = gCfg.StraightHeightClose;
        p.StraightHeightHigh = gCfg.StraightHeightHigh;

        p.SkidSideOffset = gCfg.SkidSideOffset;
        p.SkidApproachVelocityLead = gCfg.SkidApproachVelocityLead;
        p.SkidApproachHeight = gCfg.SkidApproachHeight;
        p.SkidLowExtraHeight = gCfg.SkidLowExtraHeight;
        p.SkidStrikeStartDistance = gCfg.SkidStrikeStartDistance;
        p.SkidStrikeRightDotRequirement = gCfg.SkidStrikeRightDotRequirement;
        p.SkidStrikeVelocityLead = gCfg.SkidStrikeVelocityLead;
        p.SkidStrikeBackScale = gCfg.SkidStrikeBackScale;
        p.SkidAbortDistanceAheadSq = gCfg.SkidAbortDistanceAheadSq;
        p.SkidAbortDistanceBehindSq = gCfg.SkidAbortDistanceBehindSq;

        p.DestinationVelocityMultiplier = gCfg.DestinationVelocityMultiplier;
        p.DestinationVelocityBlend = gCfg.DestinationVelocityBlend;
        p.MaxChopperAccel = gCfg.MaxChopperAccel;
        p.MinChopperAccel = gCfg.MinChopperAccel;
        p.ChopperSpeedLimitRatio = gCfg.ChopperSpeedLimitRatio;
        p.OnDrivingDecelSpeedScale = gCfg.OnDrivingDecelSpeedScale;
        p.OnDrivingTurnResponseScale = gCfg.OnDrivingTurnResponseScale;
        p.OnDrivingTurnClamp = gCfg.OnDrivingTurnClamp;
        p.OnDrivingSmoothingOldWeight = gCfg.OnDrivingSmoothingOldWeight;
        p.OnDrivingSmoothingFinalScale = gCfg.OnDrivingSmoothingFinalScale;

        p.ExitFlySpeed = gCfg.ExitFlySpeed;
        p.ExitSeekUpThreshold = gCfg.ExitSeekUpThreshold;
        p.ExitSeekAheadDistance = gCfg.ExitSeekAheadDistance;
        p.ExitSeekCarReachDistanceSq = gCfg.ExitSeekCarReachDistanceSq;
        p.ExitRightNegativeScale = gCfg.ExitRightNegativeScale;
        p.ExitFlyoutBackScale = gCfg.ExitFlyoutBackScale;
        p.ExitFlyoutExtraHeight = gCfg.ExitFlyoutExtraHeight;
        p.ExitFinishHeight = gCfg.ExitFinishHeight;
        p.ExitFinishDistanceSq = gCfg.ExitFinishDistanceSq;
        p.ExitDoDrivingMode = gCfg.ExitDoDrivingMode;
    }

    static void ClampHeatProfile(HeatProfile& p) {
        // Heat profiles intentionally allow a wider tuning range than the release fallback values.
        p.SkidCooldownThreshold = Clamp(p.SkidCooldownThreshold, -10.0f, 1.0f);
        p.SkidEntryMaxDistance = Clamp(p.SkidEntryMaxDistance, 10.0f, 150.0f);
        p.SkidEntryMinDistance = Clamp(p.SkidEntryMinDistance, 0.0f, 25.0f);
        p.SkidEntryDotRequirement = Clamp(p.SkidEntryDotRequirement, -1.0f, 0.95f);
        p.SkidEntryMaxHeightDelta = Clamp(p.SkidEntryMaxHeightDelta, 2.0f, 60.0f);

        p.StraightLeadSpeedScale = Clamp(p.StraightLeadSpeedScale, 0.0f, 2.0f);
        p.StraightLeadBase = Clamp(p.StraightLeadBase, 0.0f, 100.0f);
        p.StraightLeadMax = Clamp(p.StraightLeadMax, 5.0f, 150.0f);
        p.StraightLeadSkidMultiplier = Clamp(p.StraightLeadSkidMultiplier, 0.1f, 2.0f);
        p.StraightHeightPositiveTimer = Clamp(p.StraightHeightPositiveTimer, -5.0f, 30.0f);
        p.StraightHeightClose = Clamp(p.StraightHeightClose, -5.0f, 30.0f);
        p.StraightHeightHigh = Clamp(p.StraightHeightHigh, -5.0f, 50.0f);

        p.SkidSideOffset = Clamp(p.SkidSideOffset, 0.0f, 12.0f);
        p.SkidApproachVelocityLead = Clamp(p.SkidApproachVelocityLead, 0.0f, 1.0f);
        p.SkidApproachHeight = Clamp(p.SkidApproachHeight, -5.0f, 15.0f);
        p.SkidLowExtraHeight = Clamp(p.SkidLowExtraHeight, 0.0f, 15.0f);
        p.SkidStrikeStartDistance = Clamp(p.SkidStrikeStartDistance, 1.0f, 30.0f);
        p.SkidStrikeRightDotRequirement = Clamp(p.SkidStrikeRightDotRequirement, 0.0f, 10.0f);
        p.SkidStrikeVelocityLead = Clamp(p.SkidStrikeVelocityLead, 0.0f, 1.0f);
        p.SkidStrikeBackScale = Clamp(p.SkidStrikeBackScale, -2.0f, 2.0f);
        p.SkidAbortDistanceAheadSq = Clamp(p.SkidAbortDistanceAheadSq, 100.0f, 10000.0f);
        p.SkidAbortDistanceBehindSq = Clamp(p.SkidAbortDistanceBehindSq, 25.0f, 5000.0f);

        p.DestinationVelocityMultiplier = Clamp(p.DestinationVelocityMultiplier, 1.0f, 30.0f);
        p.DestinationVelocityBlend = Clamp(p.DestinationVelocityBlend, 0.0f, 1.0f);
        p.MaxChopperAccel = Clamp(p.MaxChopperAccel, 40.0f, 600.0f);
        p.MinChopperAccel = Clamp(p.MinChopperAccel, 0.0f, 400.0f);
        p.ChopperSpeedLimitRatio = Clamp(p.ChopperSpeedLimitRatio, 0.5f, 20.0f);
        p.OnDrivingDecelSpeedScale = Clamp(p.OnDrivingDecelSpeedScale, 0.0f, 5.0f);
        p.OnDrivingTurnResponseScale = Clamp(p.OnDrivingTurnResponseScale, -80.0f, -0.5f);
        p.OnDrivingTurnClamp = Clamp(p.OnDrivingTurnClamp, 0.4f, 12.0f);
        p.OnDrivingSmoothingOldWeight = Clamp(p.OnDrivingSmoothingOldWeight, 0.0f, 10.0f);
        p.OnDrivingSmoothingFinalScale = Clamp(p.OnDrivingSmoothingFinalScale, 0.01f, 1.0f);

        p.ExitFlySpeed = Clamp(p.ExitFlySpeed, 40.0f, 220.0f);
        p.ExitSeekUpThreshold = Clamp(p.ExitSeekUpThreshold, 0.0f, 80.0f);
        p.ExitSeekAheadDistance = Clamp(p.ExitSeekAheadDistance, 0.0f, 150.0f);
        p.ExitSeekCarReachDistanceSq = Clamp(p.ExitSeekCarReachDistanceSq, 1.0f, 2500.0f);
        p.ExitRightNegativeScale = Clamp(p.ExitRightNegativeScale, -30.0f, 0.0f);
        p.ExitFlyoutBackScale = Clamp(p.ExitFlyoutBackScale, -300.0f, 0.0f);
        p.ExitFlyoutExtraHeight = Clamp(p.ExitFlyoutExtraHeight, -15.0f, 30.0f);
        p.ExitFinishHeight = Clamp(p.ExitFinishHeight, 0.0f, 500.0f);
        p.ExitFinishDistanceSq = Clamp(p.ExitFinishDistanceSq, 25.0f, 1000000.0f);
        if (p.ExitDoDrivingMode > 15) p.ExitDoDrivingMode = 7;
    }

    static void LoadProfileFromSection(const char* path, const char* section, HeatProfile& p) {
        p.enabled = ReadIniBoolSection(path, section, "Enabled", p.enabled);

        p.SkidCooldownThreshold = ReadIniFloatSection(path, section, "SkidCooldownThreshold", p.SkidCooldownThreshold);
        p.SkidEntryMaxDistance = ReadIniFloatSection(path, section, "SkidEntryMaxDistance", p.SkidEntryMaxDistance);
        p.SkidEntryMinDistance = ReadIniFloatSection(path, section, "SkidEntryMinDistance", p.SkidEntryMinDistance);
        p.SkidEntryDotRequirement = ReadIniFloatSection(path, section, "SkidEntryDotRequirement", p.SkidEntryDotRequirement);
        p.SkidEntryMaxHeightDelta = ReadIniFloatSection(path, section, "SkidEntryMaxHeightDelta", p.SkidEntryMaxHeightDelta);

        p.StraightLeadSpeedScale = ReadIniFloatSection(path, section, "StraightLeadSpeedScale", p.StraightLeadSpeedScale);
        p.StraightLeadBase = ReadIniFloatSection(path, section, "StraightLeadBase", p.StraightLeadBase);
        p.StraightLeadMax = ReadIniFloatSection(path, section, "StraightLeadMax", p.StraightLeadMax);
        p.StraightLeadSkidMultiplier = ReadIniFloatSection(path, section, "StraightLeadSkidMultiplier", p.StraightLeadSkidMultiplier);
        p.StraightHeightPositiveTimer = ReadIniFloatSection(path, section, "StraightHeightPositiveTimer", p.StraightHeightPositiveTimer);
        p.StraightHeightClose = ReadIniFloatSection(path, section, "StraightHeightClose", p.StraightHeightClose);
        p.StraightHeightHigh = ReadIniFloatSection(path, section, "StraightHeightHigh", p.StraightHeightHigh);

        p.SkidSideOffset = ReadIniFloatSection(path, section, "SkidSideOffset", p.SkidSideOffset);
        p.SkidApproachVelocityLead = ReadIniFloatSection(path, section, "SkidApproachVelocityLead", p.SkidApproachVelocityLead);
        p.SkidApproachHeight = ReadIniFloatSection(path, section, "SkidApproachHeight", p.SkidApproachHeight);
        p.SkidLowExtraHeight = ReadIniFloatSection(path, section, "SkidLowExtraHeight", p.SkidLowExtraHeight);
        p.SkidStrikeStartDistance = ReadIniFloatSection(path, section, "SkidStrikeStartDistance", p.SkidStrikeStartDistance);
        p.SkidStrikeRightDotRequirement = ReadIniFloatSection(path, section, "SkidStrikeRightDotRequirement", p.SkidStrikeRightDotRequirement);
        p.SkidStrikeVelocityLead = ReadIniFloatSection(path, section, "SkidStrikeVelocityLead", p.SkidStrikeVelocityLead);
        p.SkidStrikeBackScale = ReadIniFloatSection(path, section, "SkidStrikeBackScale", p.SkidStrikeBackScale);
        p.SkidAbortDistanceAheadSq = ReadIniFloatSection(path, section, "SkidAbortDistanceAheadSq", p.SkidAbortDistanceAheadSq);
        p.SkidAbortDistanceBehindSq = ReadIniFloatSection(path, section, "SkidAbortDistanceBehindSq", p.SkidAbortDistanceBehindSq);

        p.DestinationVelocityMultiplier = ReadIniFloatSection(path, section, "DestinationVelocityMultiplier", p.DestinationVelocityMultiplier);
        p.DestinationVelocityBlend = ReadIniFloatSection(path, section, "DestinationVelocityBlend", p.DestinationVelocityBlend);
        p.MaxChopperAccel = ReadIniFloatSection(path, section, "MaxChopperAccel", p.MaxChopperAccel);
        p.MinChopperAccel = ReadIniFloatSection(path, section, "MinChopperAccel", p.MinChopperAccel);
        p.ChopperSpeedLimitRatio = ReadIniFloatSection(path, section, "ChopperSpeedLimitRatio", p.ChopperSpeedLimitRatio);
        p.OnDrivingDecelSpeedScale = ReadIniFloatSection(path, section, "OnDrivingDecelSpeedScale", p.OnDrivingDecelSpeedScale);
        p.OnDrivingTurnResponseScale = ReadIniFloatSection(path, section, "OnDrivingTurnResponseScale", p.OnDrivingTurnResponseScale);
        p.OnDrivingTurnClamp = ReadIniFloatSection(path, section, "OnDrivingTurnClamp", p.OnDrivingTurnClamp);
        p.OnDrivingSmoothingOldWeight = ReadIniFloatSection(path, section, "OnDrivingSmoothingOldWeight", p.OnDrivingSmoothingOldWeight);
        p.OnDrivingSmoothingFinalScale = ReadIniFloatSection(path, section, "OnDrivingSmoothingFinalScale", p.OnDrivingSmoothingFinalScale);

        p.ExitFlySpeed = ReadIniFloatSection(path, section, "ExitFlySpeed", p.ExitFlySpeed);
        p.ExitSeekUpThreshold = ReadIniFloatSection(path, section, "ExitSeekUpThreshold", p.ExitSeekUpThreshold);
        p.ExitSeekAheadDistance = ReadIniFloatSection(path, section, "ExitSeekAheadDistance", p.ExitSeekAheadDistance);
        p.ExitSeekCarReachDistanceSq = ReadIniFloatSection(path, section, "ExitSeekCarReachDistanceSq", p.ExitSeekCarReachDistanceSq);
        p.ExitRightNegativeScale = ReadIniFloatSection(path, section, "ExitRightNegativeScale", p.ExitRightNegativeScale);
        p.ExitFlyoutBackScale = ReadIniFloatSection(path, section, "ExitFlyoutBackScale", p.ExitFlyoutBackScale);
        p.ExitFlyoutExtraHeight = ReadIniFloatSection(path, section, "ExitFlyoutExtraHeight", p.ExitFlyoutExtraHeight);
        p.ExitFinishHeight = ReadIniFloatSection(path, section, "ExitFinishHeight", p.ExitFinishHeight);
        p.ExitFinishDistanceSq = ReadIniFloatSection(path, section, "ExitFinishDistanceSq", p.ExitFinishDistanceSq);
        p.ExitDoDrivingMode = static_cast<uint8_t>(GetPrivateProfileIntA(section, "ExitDoDrivingMode", p.ExitDoDrivingMode, path));
        ClampHeatProfile(p);
    }

    static void LoadHeatProfiles(const char* path) {
        for (uint32_t heat = 1; heat <= 10; ++heat) {
            HeatProfile& roam = gHeatProfiles[heat];
            FillHeatProfileFromCurrentConfig(roam);

            char section[40]{};
            std::snprintf(section, sizeof(section), "CopHeliHeat%u", heat);
            LoadProfileFromSection(path, section, roam);
            std::snprintf(section, sizeof(section), "CopHeliHeat%02u", heat);
            LoadProfileFromSection(path, section, roam);
            std::snprintf(section, sizeof(section), "CopHeli:heat%02u", heat);
            LoadProfileFromSection(path, section, roam);

            // Mirrors Bartender's heat01/race01 split. Race starts as heat, then race aliases override it.
            HeatProfile& race = gRaceProfiles[heat];
            race = roam;
            std::snprintf(section, sizeof(section), "CopHeliRace%u", heat);
            LoadProfileFromSection(path, section, race);
            std::snprintf(section, sizeof(section), "CopHeliRace%02u", heat);
            LoadProfileFromSection(path, section, race);
            std::snprintf(section, sizeof(section), "CopHeli:race%02u", heat);
            LoadProfileFromSection(path, section, race);
        }
    }

    static void LoadConfig() {
        char path[MAX_PATH]{};
        if (!ResolveIniPath(path, MAX_PATH)) {
            Log("Could not resolve INI path; using defaults.");
        }
        else {
            // IMPORTANT:
            // The public INI is organized into real sections like [Patches::Core] and [Chopper::Movement].
            // Keep root [HelicopterOptions] aliases as fallback so older INIs still work.
            const char* SEC_ROOT = "HelicopterOptions";
            const char* SEC_CORE = "Patches::Core";
            const char* SEC_SKID = "Chopper::Skid";
            const char* SEC_LEAD = "Chopper::LeadHeight";
            const char* SEC_MOVE = "Chopper::Movement";
            const char* SEC_EXIT = "Chopper::Exit";
            const char* SEC_SHEET = "Chopper::HeliSheet";
            const char* SEC_SPEED = "Chopper::Speed";
            const char* SEC_VISION = "Chopper::Vision";
            const char* SEC_RENDER = "Risky::Render";
            const char* SEC_EXTRAS = "Risky::Extras";
            const char* SEC_DISPATCH = "Risky::Dispatch";

            gCfg.LogToFile = ReadIniBoolSection(path, SEC_ROOT, "LogToFile", ReadIniBool(path, "LogToFile", gCfg.LogToFile));
            gCfg.VerboseLogging = ReadIniBoolSection(path, SEC_ROOT, "VerboseLogging", ReadIniBool(path, "VerboseLogging", gCfg.VerboseLogging));
            gLogToFile = gCfg.LogToFile;
            gVerboseLogging = gCfg.VerboseLogging;

            gCfg.EnableSkidLogicPatches = ReadIniBoolSection(path, SEC_CORE, "EnableSkidLogicPatches", ReadIniBool(path, "EnableSkidLogicPatches", gCfg.EnableSkidLogicPatches));
            gCfg.ForceSkidHitAttribute = ReadIniBoolSection(path, SEC_CORE, "ForceSkidHitAttribute", ReadIniBool(path, "ForceSkidHitAttribute", gCfg.ForceSkidHitAttribute));
            gCfg.EnableSkidEntryGatePatches = ReadIniBoolSection(path, SEC_CORE, "EnableSkidEntryGatePatches", ReadIniBool(path, "EnableSkidEntryGatePatches", gCfg.EnableSkidEntryGatePatches));
            gCfg.EnableSkidStrikePatches = ReadIniBoolSection(path, SEC_CORE, "EnableSkidStrikePatches", ReadIniBool(path, "EnableSkidStrikePatches", gCfg.EnableSkidStrikePatches));
            gCfg.EnableLeadAndHeightPatches = ReadIniBoolSection(path, SEC_CORE, "EnableLeadAndHeightPatches", ReadIniBool(path, "EnableLeadAndHeightPatches", gCfg.EnableLeadAndHeightPatches));
            gCfg.EnableAIVehicleVelocityPatches = ReadIniBoolSection(path, SEC_CORE, "EnableAIVehicleVelocityPatches", ReadIniBool(path, "EnableAIVehicleVelocityPatches", gCfg.EnableAIVehicleVelocityPatches));
            gCfg.EnableAIVehicleOnDrivingDataPatches = ReadIniBoolSection(path, SEC_CORE, "EnableAIVehicleOnDrivingDataPatches", ReadIniBool(path, "EnableAIVehicleOnDrivingDataPatches", gCfg.EnableAIVehicleOnDrivingDataPatches));
            gCfg.EnableAIVehicleOnDrivingTurnPatches = ReadIniBoolSection(path, SEC_CORE, "EnableAIVehicleOnDrivingTurnPatches", ReadIniBool(path, "EnableAIVehicleOnDrivingTurnPatches", gCfg.EnableAIVehicleOnDrivingTurnPatches));
            gCfg.EnableAIVehicleOnDrivingSmoothingPatches = ReadIniBoolSection(path, SEC_CORE, "EnableAIVehicleOnDrivingSmoothingPatches", ReadIniBool(path, "EnableAIVehicleOnDrivingSmoothingPatches", gCfg.EnableAIVehicleOnDrivingSmoothingPatches));
            gCfg.ForceOnDrivingMaxSpeedCap = ReadIniBoolSection(path, SEC_CORE, "ForceOnDrivingMaxSpeedCap", ReadIniBool(path, "ForceOnDrivingMaxSpeedCap", gCfg.ForceOnDrivingMaxSpeedCap));

            gCfg.SkidCooldownThreshold = ReadIniFloatSection(path, SEC_SKID, "SkidCooldownThreshold", ReadIniFloat(path, "SkidCooldownThreshold", gCfg.SkidCooldownThreshold));
            gCfg.SkidEntryMaxDistance = ReadIniFloatSection(path, SEC_SKID, "SkidEntryMaxDistance", ReadIniFloat(path, "SkidEntryMaxDistance", gCfg.SkidEntryMaxDistance));
            gCfg.SkidEntryMinDistance = ReadIniFloatSection(path, SEC_SKID, "SkidEntryMinDistance", ReadIniFloat(path, "SkidEntryMinDistance", gCfg.SkidEntryMinDistance));
            gCfg.SkidEntryDotRequirement = ReadIniFloatSection(path, SEC_SKID, "SkidEntryDotRequirement", ReadIniFloat(path, "SkidEntryDotRequirement", gCfg.SkidEntryDotRequirement));
            gCfg.SkidEntryMaxHeightDelta = ReadIniFloatSection(path, SEC_SKID, "SkidEntryMaxHeightDelta", ReadIniFloat(path, "SkidEntryMaxHeightDelta", gCfg.SkidEntryMaxHeightDelta));
            gCfg.SkidSideOffset = ReadIniFloatSection(path, SEC_SKID, "SkidSideOffset", ReadIniFloat(path, "SkidSideOffset", gCfg.SkidSideOffset));
            gCfg.SkidApproachVelocityLead = ReadIniFloatSection(path, SEC_SKID, "SkidApproachVelocityLead", ReadIniFloat(path, "SkidApproachVelocityLead", gCfg.SkidApproachVelocityLead));
            gCfg.SkidApproachHeight = ReadIniFloatSection(path, SEC_SKID, "SkidApproachHeight", ReadIniFloat(path, "SkidApproachHeight", gCfg.SkidApproachHeight));
            gCfg.SkidLowExtraHeight = ReadIniFloatSection(path, SEC_SKID, "SkidLowExtraHeight", ReadIniFloat(path, "SkidLowExtraHeight", gCfg.SkidLowExtraHeight));
            gCfg.SkidStrikeStartDistance = ReadIniFloatSection(path, SEC_SKID, "SkidStrikeStartDistance", ReadIniFloat(path, "SkidStrikeStartDistance", gCfg.SkidStrikeStartDistance));
            gCfg.SkidStrikeRightDotRequirement = ReadIniFloatSection(path, SEC_SKID, "SkidStrikeRightDotRequirement", ReadIniFloat(path, "SkidStrikeRightDotRequirement", gCfg.SkidStrikeRightDotRequirement));
            gCfg.SkidStrikeVelocityLead = ReadIniFloatSection(path, SEC_SKID, "SkidStrikeVelocityLead", ReadIniFloat(path, "SkidStrikeVelocityLead", gCfg.SkidStrikeVelocityLead));
            gCfg.SkidStrikeBackScale = ReadIniFloatSection(path, SEC_SKID, "SkidStrikeBackScale", ReadIniFloat(path, "SkidStrikeBackScale", gCfg.SkidStrikeBackScale));
            gCfg.SkidAbortDistanceAheadSq = ReadIniFloatSection(path, SEC_SKID, "SkidAbortDistanceAheadSq", ReadIniFloat(path, "SkidAbortDistanceAheadSq", gCfg.SkidAbortDistanceAheadSq));
            gCfg.SkidAbortDistanceBehindSq = ReadIniFloatSection(path, SEC_SKID, "SkidAbortDistanceBehindSq", ReadIniFloat(path, "SkidAbortDistanceBehindSq", gCfg.SkidAbortDistanceBehindSq));

            gCfg.StraightLeadSpeedScale = ReadIniFloatSection(path, SEC_LEAD, "StraightLeadSpeedScale", ReadIniFloat(path, "StraightLeadSpeedScale", gCfg.StraightLeadSpeedScale));
            gCfg.StraightLeadBase = ReadIniFloatSection(path, SEC_LEAD, "StraightLeadBase", ReadIniFloat(path, "StraightLeadBase", gCfg.StraightLeadBase));
            gCfg.StraightLeadMax = ReadIniFloatSection(path, SEC_LEAD, "StraightLeadMax", ReadIniFloat(path, "StraightLeadMax", gCfg.StraightLeadMax));
            gCfg.StraightLeadSkidMultiplier = ReadIniFloatSection(path, SEC_LEAD, "StraightLeadSkidMultiplier", ReadIniFloat(path, "StraightLeadSkidMultiplier", gCfg.StraightLeadSkidMultiplier));
            gCfg.StraightHeightPositiveTimer = ReadIniFloatSection(path, SEC_LEAD, "StraightHeightPositiveTimer", ReadIniFloat(path, "StraightHeightPositiveTimer", gCfg.StraightHeightPositiveTimer));
            gCfg.StraightHeightClose = ReadIniFloatSection(path, SEC_LEAD, "StraightHeightClose", ReadIniFloat(path, "StraightHeightClose", gCfg.StraightHeightClose));
            gCfg.StraightHeightHigh = ReadIniFloatSection(path, SEC_LEAD, "StraightHeightHigh", ReadIniFloat(path, "StraightHeightHigh", gCfg.StraightHeightHigh));

            gCfg.DestinationVelocityMultiplier = ReadIniFloatSection(path, SEC_MOVE, "DestinationVelocityMultiplier", ReadIniFloat(path, "DestinationVelocityMultiplier", gCfg.DestinationVelocityMultiplier));
            gCfg.DestinationVelocityBlend = ReadIniFloatSection(path, SEC_MOVE, "DestinationVelocityBlend", ReadIniFloat(path, "DestinationVelocityBlend", gCfg.DestinationVelocityBlend));
            gCfg.MaxChopperAccel = ReadIniFloatSection(path, SEC_MOVE, "MaxChopperAccel", ReadIniFloat(path, "MaxChopperAccel", gCfg.MaxChopperAccel));
            gCfg.MinChopperAccel = ReadIniFloatSection(path, SEC_MOVE, "MinChopperAccel", ReadIniFloat(path, "MinChopperAccel", gCfg.MinChopperAccel));
            gCfg.ChopperSpeedLimitRatio = ReadIniFloatSection(path, SEC_MOVE, "ChopperSpeedLimitRatio", ReadIniFloat(path, "ChopperSpeedLimitRatio", gCfg.ChopperSpeedLimitRatio));
            gCfg.OnDrivingDecelSpeedScale = ReadIniFloatSection(path, SEC_MOVE, "OnDrivingDecelSpeedScale", ReadIniFloat(path, "OnDrivingDecelSpeedScale", gCfg.OnDrivingDecelSpeedScale));
            gCfg.OnDrivingTurnResponseScale = ReadIniFloatSection(path, SEC_MOVE, "OnDrivingTurnResponseScale", ReadIniFloat(path, "OnDrivingTurnResponseScale", gCfg.OnDrivingTurnResponseScale));
            gCfg.OnDrivingTurnClamp = ReadIniFloatSection(path, SEC_MOVE, "OnDrivingTurnClamp", ReadIniFloat(path, "OnDrivingTurnClamp", gCfg.OnDrivingTurnClamp));
            gCfg.OnDrivingSmoothingOldWeight = ReadIniFloatSection(path, SEC_MOVE, "OnDrivingSmoothingOldWeight", ReadIniFloat(path, "OnDrivingSmoothingOldWeight", gCfg.OnDrivingSmoothingOldWeight));
            gCfg.OnDrivingSmoothingFinalScale = ReadIniFloatSection(path, SEC_MOVE, "OnDrivingSmoothingFinalScale", ReadIniFloat(path, "OnDrivingSmoothingFinalScale", gCfg.OnDrivingSmoothingFinalScale));

            gCfg.DisableFuelBasedHeliExit = ReadIniBoolSection(path, SEC_EXIT, "DisableFuelBasedHeliExit", ReadIniBool(path, "DisableFuelBasedHeliExit", gCfg.DisableFuelBasedHeliExit));
            gCfg.EnableHeliExitActionPatches = ReadIniBoolSection(path, SEC_EXIT, "EnableHeliExitActionPatches", ReadIniBool(path, "EnableHeliExitActionPatches", gCfg.EnableHeliExitActionPatches));
            gCfg.PatchExitFlySpeed = ReadIniBoolSection(path, SEC_EXIT, "PatchExitFlySpeed", ReadIniBool(path, "PatchExitFlySpeed", gCfg.PatchExitFlySpeed));
            gCfg.PatchExitSeekUpThreshold = ReadIniBoolSection(path, SEC_EXIT, "PatchExitSeekUpThreshold", ReadIniBool(path, "PatchExitSeekUpThreshold", gCfg.PatchExitSeekUpThreshold));
            gCfg.PatchExitSeekAheadDistance = ReadIniBoolSection(path, SEC_EXIT, "PatchExitSeekAheadDistance", ReadIniBool(path, "PatchExitSeekAheadDistance", gCfg.PatchExitSeekAheadDistance));
            gCfg.PatchExitSeekCarReachDistance = ReadIniBoolSection(path, SEC_EXIT, "PatchExitSeekCarReachDistance", ReadIniBool(path, "PatchExitSeekCarReachDistance", gCfg.PatchExitSeekCarReachDistance));
            gCfg.PatchExitFlyoutBackScale = ReadIniBoolSection(path, SEC_EXIT, "PatchExitFlyoutBackScale", ReadIniBool(path, "PatchExitFlyoutBackScale", gCfg.PatchExitFlyoutBackScale));
            gCfg.PatchExitFlyoutExtraHeight = ReadIniBoolSection(path, SEC_EXIT, "PatchExitFlyoutExtraHeight", ReadIniBool(path, "PatchExitFlyoutExtraHeight", gCfg.PatchExitFlyoutExtraHeight));
            gCfg.PatchExitFinishRules = ReadIniBoolSection(path, SEC_EXIT, "PatchExitFinishRules", ReadIniBool(path, "PatchExitFinishRules", gCfg.PatchExitFinishRules));
            gCfg.PatchExitDoDrivingMode = ReadIniBoolSection(path, SEC_EXIT, "PatchExitDoDrivingMode", ReadIniBool(path, "PatchExitDoDrivingMode", gCfg.PatchExitDoDrivingMode));
            gCfg.ExitFlySpeed = ReadIniFloatSection(path, SEC_EXIT, "ExitFlySpeed", ReadIniFloat(path, "ExitFlySpeed", gCfg.ExitFlySpeed));
            gCfg.ExitSeekUpThreshold = ReadIniFloatSection(path, SEC_EXIT, "ExitSeekUpThreshold", ReadIniFloat(path, "ExitSeekUpThreshold", gCfg.ExitSeekUpThreshold));
            gCfg.ExitSeekAheadDistance = ReadIniFloatSection(path, SEC_EXIT, "ExitSeekAheadDistance", ReadIniFloat(path, "ExitSeekAheadDistance", gCfg.ExitSeekAheadDistance));
            gCfg.ExitSeekCarReachDistanceSq = ReadIniFloatSection(path, SEC_EXIT, "ExitSeekCarReachDistanceSq", ReadIniFloat(path, "ExitSeekCarReachDistanceSq", gCfg.ExitSeekCarReachDistanceSq));
            gCfg.ExitRightNegativeScale = ReadIniFloatSection(path, SEC_EXIT, "ExitRightNegativeScale", ReadIniFloat(path, "ExitRightNegativeScale", gCfg.ExitRightNegativeScale));
            gCfg.ExitFlyoutBackScale = ReadIniFloatSection(path, SEC_EXIT, "ExitFlyoutBackScale", ReadIniFloat(path, "ExitFlyoutBackScale", gCfg.ExitFlyoutBackScale));
            gCfg.ExitFlyoutExtraHeight = ReadIniFloatSection(path, SEC_EXIT, "ExitFlyoutExtraHeight", ReadIniFloat(path, "ExitFlyoutExtraHeight", gCfg.ExitFlyoutExtraHeight));
            gCfg.ExitFinishHeight = ReadIniFloatSection(path, SEC_EXIT, "ExitFinishHeight", ReadIniFloat(path, "ExitFinishHeight", gCfg.ExitFinishHeight));
            gCfg.ExitFinishDistanceSq = ReadIniFloatSection(path, SEC_EXIT, "ExitFinishDistanceSq", ReadIniFloat(path, "ExitFinishDistanceSq", gCfg.ExitFinishDistanceSq));
            gCfg.ExitDoDrivingMode = static_cast<uint8_t>(GetPrivateProfileIntA(SEC_EXIT, "ExitDoDrivingMode", GetPrivateProfileIntA(SEC_ROOT, "ExitDoDrivingMode", gCfg.ExitDoDrivingMode, path), path));

            gCfg.EnableHeliSheetPatches = ReadIniBoolSection(path, SEC_SHEET, "EnableHeliSheetPatches", ReadIniBool(path, "EnableHeliSheetPatches", gCfg.EnableHeliSheetPatches));
            gCfg.ForceIgnoreHeliSheetInAllPursuitModes = ReadIniBoolSection(path, SEC_SHEET, "ForceIgnoreHeliSheetInAllPursuitModes", ReadIniBool(path, "ForceIgnoreHeliSheetInAllPursuitModes", gCfg.ForceIgnoreHeliSheetInAllPursuitModes));
            gCfg.RespectHeliSheetDuringSkid = ReadIniBoolSection(path, SEC_SHEET, "RespectHeliSheetDuringSkid", ReadIniBool(path, "RespectHeliSheetDuringSkid", gCfg.RespectHeliSheetDuringSkid));

            gCfg.EnableChopperSpeed = ReadIniBoolSection(path, SEC_SPEED, "Enable", ReadIniBool(path, "EnableChopperSpeed", gCfg.EnableChopperSpeed));
            gCfg.ChopperSpeedFactor = ReadIniFloatSection(path, SEC_SPEED, "SpeedFactor", ReadIniFloat(path, "ChopperSpeedFactor", gCfg.ChopperSpeedFactor));
            gCfg.ChopperMaxSpeed = ReadIniFloatSection(path, SEC_SPEED, "MaxSpeed", ReadIniFloat(path, "ChopperMaxSpeed", gCfg.ChopperMaxSpeed));
            gCfg.ChopperSpeedPush = ReadIniFloatSection(path, SEC_SPEED, "SpeedPush", ReadIniFloat(path, "ChopperSpeedPush", gCfg.ChopperSpeedPush));
            gCfg.ChopperOverspeedBrake = ReadIniFloatSection(path, SEC_SPEED, "OverspeedBrake", ReadIniFloat(path, "ChopperOverspeedBrake", gCfg.ChopperOverspeedBrake));
            gCfg.ChopperMinApplySpeed = ReadIniFloatSection(path, SEC_SPEED, "MinApplySpeed", ReadIniFloat(path, "ChopperMinApplySpeed", gCfg.ChopperMinApplySpeed));
            gCfg.ChopperTurnSlowdown = ReadIniBoolSection(path, SEC_SPEED, "TurnSlowdown", ReadIniBool(path, "ChopperTurnSlowdown", gCfg.ChopperTurnSlowdown));
            gCfg.ChopperTurnSlowdownStrength = ReadIniFloatSection(path, SEC_SPEED, "TurnSlowdownStrength", ReadIniFloat(path, "ChopperTurnSlowdownStrength", gCfg.ChopperTurnSlowdownStrength));
            gCfg.ChopperIgnoreNegativeDesiredSpeed = ReadIniBoolSection(path, SEC_SPEED, "IgnoreNegativeDesiredSpeed", ReadIniBool(path, "ChopperIgnoreNegativeDesiredSpeed", gCfg.ChopperIgnoreNegativeDesiredSpeed));

            gCfg.EnableVisionPatches = ReadIniBoolSection(path, SEC_VISION, "Enable", ReadIniBool(path, "EnableVisionPatches", gCfg.EnableVisionPatches));
            gCfg.HeliSeesThroughWalls = ReadIniBoolSection(path, SEC_VISION, "SeeThroughWalls", ReadIniBool(path, "HeliSeesThroughWalls", gCfg.HeliSeesThroughWalls));

            gCfg.EnableLegacyRenderDistancePatch = ReadIniBoolSection(path, SEC_RENDER, "EnableLegacyRenderDistancePatch", ReadIniBool(path, "EnableLegacyRenderDistancePatch", gCfg.EnableLegacyRenderDistancePatch));
            gCfg.LegacyRenderDistanceThreshold = ReadIniFloatSection(path, SEC_RENDER, "LegacyRenderDistanceThreshold", ReadIniFloat(path, "LegacyRenderDistanceThreshold", gCfg.LegacyRenderDistanceThreshold));
            gCfg.EnableHeliRenderConnPatches = ReadIniBoolSection(path, SEC_RENDER, "EnableHeliRenderConnPatches", ReadIniBool(path, "EnableHeliRenderConnPatches", gCfg.EnableHeliRenderConnPatches));
            gCfg.ForceHeliRenderServiceInView = ReadIniBoolSection(path, SEC_RENDER, "ForceHeliRenderServiceInView", ReadIniBool(path, "ForceHeliRenderServiceInView", gCfg.ForceHeliRenderServiceInView));
            gCfg.ForceHeliRenderServiceDistance = ReadIniBoolSection(path, SEC_RENDER, "ForceHeliRenderServiceDistance", ReadIniBool(path, "ForceHeliRenderServiceDistance", gCfg.ForceHeliRenderServiceDistance));
            gCfg.PreventHeliRenderHideOnServiceFail = ReadIniBoolSection(path, SEC_RENDER, "PreventHeliRenderHideOnServiceFail", ReadIniBool(path, "PreventHeliRenderHideOnServiceFail", gCfg.PreventHeliRenderHideOnServiceFail));
            gCfg.PatchHeliRenderResetDistance = ReadIniBoolSection(path, SEC_RENDER, "PatchHeliRenderResetDistance", ReadIniBool(path, "PatchHeliRenderResetDistance", gCfg.PatchHeliRenderResetDistance));
            gCfg.HeliRenderServiceDistance = ReadIniFloatSection(path, SEC_RENDER, "HeliRenderServiceDistance", ReadIniFloat(path, "HeliRenderServiceDistance", gCfg.HeliRenderServiceDistance));
            gCfg.HeliRenderResetDistance = ReadIniFloatSection(path, SEC_RENDER, "HeliRenderResetDistance", ReadIniFloat(path, "HeliRenderResetDistance", gCfg.HeliRenderResetDistance));

            gCfg.EnableGhidraExtraPatches = ReadIniBoolSection(path, SEC_EXTRAS, "EnableGhidraExtraPatches", ReadIniBool(path, "EnableGhidraExtraPatches", gCfg.EnableGhidraExtraPatches));
            gCfg.ForceAIGoalHeliRoadBlockSelection = ReadIniBoolSection(path, SEC_EXTRAS, "ForceAIGoalHeliRoadBlockSelection", ReadIniBool(path, "ForceAIGoalHeliRoadBlockSelection", gCfg.ForceAIGoalHeliRoadBlockSelection));
            gCfg.EnableHeliWashPatches = ReadIniBoolSection(path, SEC_EXTRAS, "EnableHeliWashPatches", ReadIniBool(path, "EnableHeliWashPatches", gCfg.EnableHeliWashPatches));
            gCfg.HeliWashPhysicsStrength = ReadIniFloatSection(path, SEC_EXTRAS, "HeliWashPhysicsStrength", ReadIniFloat(path, "HeliWashPhysicsStrength", gCfg.HeliWashPhysicsStrength));

            gCfg.EnableDispatchSpawnPatches = ReadIniBoolSection(path, SEC_DISPATCH, "EnableDispatchSpawnPatches", ReadIniBool(path, "EnableDispatchSpawnPatches", gCfg.EnableDispatchSpawnPatches));
            gCfg.AllowCopheliWeightedSelection = ReadIniBoolSection(path, SEC_DISPATCH, "AllowCopheliWeightedSelection", ReadIniBool(path, "AllowCopheliWeightedSelection", gCfg.AllowCopheliWeightedSelection));
            gCfg.IgnoreExistingHeliForSelectorTrigger = ReadIniBoolSection(path, SEC_DISPATCH, "IgnoreExistingHeliForSelectorTrigger", ReadIniBool(path, "IgnoreExistingHeliForSelectorTrigger", gCfg.IgnoreExistingHeliForSelectorTrigger));
            gCfg.IgnoreExistingHeliForSpecialSpawn = ReadIniBoolSection(path, SEC_DISPATCH, "IgnoreExistingHeliForSpecialSpawn", ReadIniBool(path, "IgnoreExistingHeliForSpecialSpawn", gCfg.IgnoreExistingHeliForSpecialSpawn));
            gCfg.ForceWeightedSelectorAlwaysReturnCopheli = ReadIniBoolSection(path, SEC_DISPATCH, "ForceWeightedSelectorAlwaysReturnCopheli", ReadIniBool(path, "ForceWeightedSelectorAlwaysReturnCopheli", gCfg.ForceWeightedSelectorAlwaysReturnCopheli));
            gCfg.BypassSpawnCapForImmediateRequests = ReadIniBoolSection(path, SEC_DISPATCH, "BypassSpawnCapForImmediateRequests", ReadIniBool(path, "BypassSpawnCapForImmediateRequests", gCfg.BypassSpawnCapForImmediateRequests));

            gCfg.EnableHeatBasedProfiles = ReadIniBool(path, "EnableHeatBasedProfiles", gCfg.EnableHeatBasedProfiles);
            gCfg.EnableRaceBasedProfiles = ReadIniBool(path, "EnableRaceBasedProfiles", gCfg.EnableRaceBasedProfiles);
            gCfg.InstallHeatObserverHook = ReadIniBool(path, "InstallHeatObserverHook", gCfg.InstallHeatObserverHook);
            gCfg.LogHeatProfileChanges = ReadIniBool(path, "LogHeatProfileChanges", gCfg.LogHeatProfileChanges);
            gCfg.HeatProfileStartupHeat = static_cast<uint32_t>(GetPrivateProfileIntA(SEC_ROOT, "HeatProfileStartupHeat", gCfg.HeatProfileStartupHeat, path));
            gCfg.HeatProfileStartupIsRace = ReadIniBool(path, "HeatProfileStartupIsRace", gCfg.HeatProfileStartupIsRace);
        }

        gCfg.SkidCooldownThreshold = Clamp(gCfg.SkidCooldownThreshold, -10.0f, 1.0f);
        gCfg.SkidEntryMaxDistance = Clamp(gCfg.SkidEntryMaxDistance, 10.0f, 150.0f);
        gCfg.SkidEntryMinDistance = Clamp(gCfg.SkidEntryMinDistance, 0.0f, 25.0f);
        gCfg.SkidEntryDotRequirement = Clamp(gCfg.SkidEntryDotRequirement, -1.0f, 0.95f);
        gCfg.SkidEntryMaxHeightDelta = Clamp(gCfg.SkidEntryMaxHeightDelta, 2.0f, 60.0f);
        gCfg.StraightLeadSpeedScale = Clamp(gCfg.StraightLeadSpeedScale, 0.0f, 2.0f);
        gCfg.StraightLeadBase = Clamp(gCfg.StraightLeadBase, 0.0f, 100.0f);
        gCfg.StraightLeadMax = Clamp(gCfg.StraightLeadMax, 5.0f, 150.0f);
        gCfg.StraightLeadSkidMultiplier = Clamp(gCfg.StraightLeadSkidMultiplier, 0.1f, 2.0f);
        gCfg.StraightHeightPositiveTimer = Clamp(gCfg.StraightHeightPositiveTimer, -5.0f, 30.0f);
        gCfg.StraightHeightClose = Clamp(gCfg.StraightHeightClose, -5.0f, 30.0f);
        gCfg.StraightHeightHigh = Clamp(gCfg.StraightHeightHigh, -5.0f, 50.0f);
        gCfg.SkidSideOffset = Clamp(gCfg.SkidSideOffset, 0.0f, 12.0f);
        gCfg.SkidApproachVelocityLead = Clamp(gCfg.SkidApproachVelocityLead, 0.0f, 1.0f);
        gCfg.SkidApproachHeight = Clamp(gCfg.SkidApproachHeight, -5.0f, 15.0f);
        gCfg.SkidLowExtraHeight = Clamp(gCfg.SkidLowExtraHeight, 0.0f, 15.0f);
        gCfg.SkidStrikeStartDistance = Clamp(gCfg.SkidStrikeStartDistance, 1.0f, 30.0f);
        gCfg.SkidStrikeRightDotRequirement = Clamp(gCfg.SkidStrikeRightDotRequirement, 0.0f, 10.0f);
        gCfg.SkidStrikeVelocityLead = Clamp(gCfg.SkidStrikeVelocityLead, 0.0f, 1.0f);
        gCfg.SkidStrikeBackScale = Clamp(gCfg.SkidStrikeBackScale, -2.0f, 2.0f);
        gCfg.SkidAbortDistanceAheadSq = Clamp(gCfg.SkidAbortDistanceAheadSq, 100.0f, 10000.0f);
        gCfg.SkidAbortDistanceBehindSq = Clamp(gCfg.SkidAbortDistanceBehindSq, 25.0f, 5000.0f);
        gCfg.DestinationVelocityMultiplier = Clamp(gCfg.DestinationVelocityMultiplier, 1.0f, 12.0f);
        gCfg.DestinationVelocityBlend = Clamp(gCfg.DestinationVelocityBlend, 0.0f, 1.0f);
        gCfg.MaxChopperAccel = Clamp(gCfg.MaxChopperAccel, 40.0f, 250.0f);
        gCfg.MinChopperAccel = Clamp(gCfg.MinChopperAccel, 0.0f, 160.0f);
        gCfg.ChopperSpeedLimitRatio = Clamp(gCfg.ChopperSpeedLimitRatio, 0.5f, 8.0f);
        gCfg.OnDrivingDecelSpeedScale = Clamp(gCfg.OnDrivingDecelSpeedScale, 0.0f, 3.0f);
        gCfg.OnDrivingTurnResponseScale = Clamp(gCfg.OnDrivingTurnResponseScale, -30.0f, -0.5f);
        gCfg.OnDrivingTurnClamp = Clamp(gCfg.OnDrivingTurnClamp, 0.4f, 5.0f);
        gCfg.OnDrivingSmoothingOldWeight = Clamp(gCfg.OnDrivingSmoothingOldWeight, 0.0f, 7.0f);
        gCfg.OnDrivingSmoothingFinalScale = Clamp(gCfg.OnDrivingSmoothingFinalScale, 0.05f, 1.0f);
        gCfg.ExitFlySpeed = Clamp(gCfg.ExitFlySpeed, 40.0f, 220.0f);
        gCfg.ExitSeekUpThreshold = Clamp(gCfg.ExitSeekUpThreshold, 0.0f, 80.0f);
        gCfg.ExitSeekAheadDistance = Clamp(gCfg.ExitSeekAheadDistance, 0.0f, 150.0f);
        gCfg.ExitSeekCarReachDistanceSq = Clamp(gCfg.ExitSeekCarReachDistanceSq, 1.0f, 2500.0f);
        gCfg.ExitRightNegativeScale = Clamp(gCfg.ExitRightNegativeScale, -30.0f, 0.0f);
        gCfg.ExitFlyoutBackScale = Clamp(gCfg.ExitFlyoutBackScale, -300.0f, 0.0f);
        gCfg.ExitFlyoutExtraHeight = Clamp(gCfg.ExitFlyoutExtraHeight, -15.0f, 30.0f);
        gCfg.ExitFinishHeight = Clamp(gCfg.ExitFinishHeight, 0.0f, 500.0f);
        gCfg.ExitFinishDistanceSq = Clamp(gCfg.ExitFinishDistanceSq, 25.0f, 1000000.0f);
        if (gCfg.ExitDoDrivingMode > 15) gCfg.ExitDoDrivingMode = 7;
        gCfg.LegacyRenderDistanceThreshold = Clamp(gCfg.LegacyRenderDistanceThreshold, -10.0f, 1.0f);
        gCfg.HeliRenderServiceDistance = Clamp(gCfg.HeliRenderServiceDistance, 0.0f, 1000000.0f);
        gCfg.HeliRenderResetDistance = Clamp(gCfg.HeliRenderResetDistance, 1.0f, 1000000.0f);
        gCfg.HeliWashPhysicsStrength = Clamp(gCfg.HeliWashPhysicsStrength, 0.0f, 10.0f);

        gCfg.ChopperSpeedFactor = Clamp(gCfg.ChopperSpeedFactor, 0.50f, 2.50f);
        gCfg.ChopperMaxSpeed = Clamp(gCfg.ChopperMaxSpeed, 0.0f, 400.0f);
        gCfg.ChopperSpeedPush = Clamp(gCfg.ChopperSpeedPush, 0.0f, 1.0f);
        gCfg.ChopperOverspeedBrake = Clamp(gCfg.ChopperOverspeedBrake, 0.0f, 1.0f);
        gCfg.ChopperMinApplySpeed = Clamp(gCfg.ChopperMinApplySpeed, 0.0f, 250.0f);
        gCfg.ChopperTurnSlowdownStrength = Clamp(gCfg.ChopperTurnSlowdownStrength, 0.0f, 500.0f);
        if (gCfg.HeatProfileStartupHeat < 1 || gCfg.HeatProfileStartupHeat > 10) gCfg.HeatProfileStartupHeat = 1;

        fSkidCooldownThreshold = gCfg.SkidCooldownThreshold;
        fSkidEntryMaxDistance = gCfg.SkidEntryMaxDistance;
        fSkidEntryMinDistance = gCfg.SkidEntryMinDistance;
        fSkidEntryDotRequirement = gCfg.SkidEntryDotRequirement;
        fSkidEntryMaxHeightDelta = gCfg.SkidEntryMaxHeightDelta;
        fStraightLeadSpeedScale = gCfg.StraightLeadSpeedScale;
        fStraightLeadBase = gCfg.StraightLeadBase;
        fStraightLeadMax = gCfg.StraightLeadMax;
        fStraightLeadSkidMultiplier = gCfg.StraightLeadSkidMultiplier;
        fStraightHeightPositiveTimer = gCfg.StraightHeightPositiveTimer;
        fStraightHeightClose = gCfg.StraightHeightClose;
        fStraightHeightHigh = gCfg.StraightHeightHigh;
        fSkidSideOffsetPositive = gCfg.SkidSideOffset;
        fSkidSideOffsetNegative = -gCfg.SkidSideOffset;
        fSkidApproachVelocityLead = gCfg.SkidApproachVelocityLead;
        fSkidApproachHeight = gCfg.SkidApproachHeight;
        fSkidLowExtraHeight = gCfg.SkidLowExtraHeight;
        fSkidStrikeStartDistance = gCfg.SkidStrikeStartDistance;
        fSkidStrikeRightDotRequirement = gCfg.SkidStrikeRightDotRequirement;
        fSkidStrikeVelocityLead = gCfg.SkidStrikeVelocityLead;
        fSkidStrikeBackScale = gCfg.SkidStrikeBackScale;
        fSkidAbortDistanceAheadSq = gCfg.SkidAbortDistanceAheadSq;
        fSkidAbortDistanceBehindSq = gCfg.SkidAbortDistanceBehindSq;
        fDestinationVelocityMultiplier = gCfg.DestinationVelocityMultiplier;
        fDestinationVelocityBlend = gCfg.DestinationVelocityBlend;
        fOnDrivingDecelSpeedScale = gCfg.OnDrivingDecelSpeedScale;
        fOnDrivingTurnResponseScale = gCfg.OnDrivingTurnResponseScale;
        fOnDrivingTurnClampPositive = gCfg.OnDrivingTurnClamp;
        fOnDrivingTurnClampNegative = -gCfg.OnDrivingTurnClamp;
        fOnDrivingSmoothingOldWeight = gCfg.OnDrivingSmoothingOldWeight;
        fOnDrivingSmoothingFinalScale = gCfg.OnDrivingSmoothingFinalScale;
        fExitFlySpeed = gCfg.ExitFlySpeed;
        fExitSeekUpThreshold = gCfg.ExitSeekUpThreshold;
        fExitSeekAheadDistance = gCfg.ExitSeekAheadDistance;
        fExitSeekCarReachDistanceSq = gCfg.ExitSeekCarReachDistanceSq;
        fExitRightNegativeScale = gCfg.ExitRightNegativeScale;
        fExitFlyoutBackScale = gCfg.ExitFlyoutBackScale;
        fExitFlyoutExtraHeight = gCfg.ExitFlyoutExtraHeight;
        fExitFinishHeight = gCfg.ExitFinishHeight;
        fExitFinishDistanceSq = gCfg.ExitFinishDistanceSq;
        fLegacyRenderDistanceThreshold = gCfg.LegacyRenderDistanceThreshold;
        fHeliRenderServiceDistance = gCfg.HeliRenderServiceDistance;
        fHeliRenderResetDistance = gCfg.HeliRenderResetDistance;
        fHeliWashPhysicsStrength = gCfg.HeliWashPhysicsStrength;

        if (gCfg.EnableHeatBasedProfiles && path[0] != '\0') {
            LoadHeatProfiles(path);
            gCurrentIsRacing = gCfg.HeatProfileStartupIsRace;
            ApplyHeatProfile(gCfg.HeatProfileStartupHeat, "startup");
        }
    }

    static void LogConfigSummary() {
        if (!gVerboseLogging) return;
        Log("Config summary: Skid=%d AIVelocity=%d OnDrivingData=%d Turn=%d Smooth=%d ForceSpeedCap=%d",
            gCfg.EnableSkidLogicPatches, gCfg.EnableAIVehicleVelocityPatches,
            gCfg.EnableAIVehicleOnDrivingDataPatches, gCfg.EnableAIVehicleOnDrivingTurnPatches,
            gCfg.EnableAIVehicleOnDrivingSmoothingPatches, gCfg.ForceOnDrivingMaxSpeedCap);
        Log("Turn/speed: TurnResponse=%.3f TurnClamp=%.3f DestMult=%.3f DestBlend=%.3f MaxAccel=%.3f MinAccel=%.3f Ratio=%.3f DecelScale=%.3f",
            gCfg.OnDrivingTurnResponseScale, gCfg.OnDrivingTurnClamp,
            gCfg.DestinationVelocityMultiplier, gCfg.DestinationVelocityBlend,
            gCfg.MaxChopperAccel, gCfg.MinChopperAccel, gCfg.ChopperSpeedLimitRatio,
            gCfg.OnDrivingDecelSpeedScale);
        Log("Dispatch: Enable=%d AllowWeighted=%d IgnoreSelectorGate=%d IgnoreSpecialGate=%d ForceCopheli=%d BypassCap=%d",
            gCfg.EnableDispatchSpawnPatches, gCfg.AllowCopheliWeightedSelection,
            gCfg.IgnoreExistingHeliForSelectorTrigger, gCfg.IgnoreExistingHeliForSpecialSpawn,
            gCfg.ForceWeightedSelectorAlwaysReturnCopheli, gCfg.BypassSpawnCapForImmediateRequests);
        Log("HeliSheet/Exit: HeliSheet=%d IgnoreSheetAll=%d DisableFuelExit=%d ExitAction=%d",
            gCfg.EnableHeliSheetPatches, gCfg.ForceIgnoreHeliSheetInAllPursuitModes,
            gCfg.DisableFuelBasedHeliExit, gCfg.EnableHeliExitActionPatches);

        Log("Chopper::Speed: Enable=%d Factor=%.3f MaxSpeed=%.3f Push=%.3f OverspeedBrake=%.3f MinApply=%.3f TurnSlowdown=%d TurnStrength=%.3f IgnoreNegative=%d",
            gCfg.EnableChopperSpeed ? 1 : 0, gCfg.ChopperSpeedFactor, gCfg.ChopperMaxSpeed,
            gCfg.ChopperSpeedPush, gCfg.ChopperOverspeedBrake, gCfg.ChopperMinApplySpeed,
            gCfg.ChopperTurnSlowdown ? 1 : 0, gCfg.ChopperTurnSlowdownStrength,
            gCfg.ChopperIgnoreNegativeDesiredSpeed ? 1 : 0);
        Log("Chopper::Vision: Enable=%d SeeThroughWalls=%d",
            gCfg.EnableVisionPatches ? 1 : 0, gCfg.HeliSeesThroughWalls ? 1 : 0);
        Log("Heat profiles: Enable=%d RaceProfiles=%d Hook=%d StartupHeat=%lu StartupRace=%d CurrentHeat=%lu CurrentRace=%d",
            gCfg.EnableHeatBasedProfiles, gCfg.EnableRaceBasedProfiles, gCfg.InstallHeatObserverHook,
            static_cast<unsigned long>(gCfg.HeatProfileStartupHeat), gCfg.HeatProfileStartupIsRace ? 1 : 0,
            static_cast<unsigned long>(gCurrentHeatLevel), gCurrentIsRacing ? 1 : 0);
    }

    static bool ValidateSpeedExe() {
        HMODULE exe = GetModuleHandleW(nullptr);
        if (!exe) return false;

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exe);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;

        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(exe) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        uintptr_t base = reinterpret_cast<uintptr_t>(exe);
        if (base != kExpectedImageBase) {
            Log("Unsupported runtime image base 0x%08lX. Expected 0x%08lX.",
                static_cast<unsigned long>(base), static_cast<unsigned long>(kExpectedImageBase));
            return false;
        }

        if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
            Log("Unsupported architecture; build/load this as Win32/x86.");
            return false;
        }

        if (nt->FileHeader.TimeDateStamp != kExpectedTimeDateStamp) {
            Log("Timestamp warning: got 0x%08lX expected 0x%08lX. Byte guards still decide every patch.",
                static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                static_cast<unsigned long>(kExpectedTimeDateStamp));
        }

        Log("EXE validated: base=0x%08lX machine=0x%04X timestamp=0x%08lX imageSize=0x%08lX",
            static_cast<unsigned long>(base),
            static_cast<unsigned>(nt->FileHeader.Machine),
            static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
            static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage));
        return true;
    }

    static bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t len) {
        return std::memcmp(reinterpret_cast<const void*>(va), expected, len) == 0;
    }

    static bool WriteMemory(void* dst, const void* src, size_t len) {
        DWORD oldProtect = 0;
        if (!VirtualProtect(dst, len, PAGE_EXECUTE_READWRITE, &oldProtect)) return false;
        std::memcpy(dst, src, len);
        FlushInstructionCache(GetCurrentProcess(), dst, len);
        DWORD ignored = 0;
        VirtualProtect(dst, len, oldProtect, &ignored);
        return true;
    }

    static bool PatchFloatOperand(const FloatOperandPatch& p) {
        if (!CheckBytes(p.insnVA, p.expected, p.expectedLen)) {
            ++gPatchSkippedCount;
            Log("%s skipped: byte guard failed at 0x%08lX.", p.name, static_cast<unsigned long>(p.insnVA));
            return false;
        }
        uint32_t newAddr = reinterpret_cast<uint32_t>(p.replacement);
        if (!WriteMemory(reinterpret_cast<void*>(p.operandVA), &newAddr, sizeof(newAddr))) {
            ++gPatchFailedCount;
            Log("%s failed: could not write operand at 0x%08lX.", p.name, static_cast<unsigned long>(p.operandVA));
            return false;
        }
        ++gPatchAppliedCount;
        Log("%s -> %.3f", p.name, *p.replacement);
        return true;
    }

    static bool PatchBytes(const char* name, uintptr_t va, const uint8_t* expected, size_t expectedLen, const uint8_t* replacement, size_t replacementLen) {
        if (!CheckBytes(va, expected, expectedLen)) {
            ++gPatchSkippedCount;
            Log("%s skipped: byte guard failed at 0x%08lX.", name, static_cast<unsigned long>(va));
            return false;
        }
        if (!WriteMemory(reinterpret_cast<void*>(va), replacement, replacementLen)) {
            ++gPatchFailedCount;
            Log("%s failed: could not write at 0x%08lX.", name, static_cast<unsigned long>(va));
            return false;
        }
        ++gPatchAppliedCount;
        Log("%s applied.", name);
        return true;
    }

    static bool PatchImm8(const char* name, uintptr_t insnVA, const uint8_t* expected, size_t expectedLen, uintptr_t immVA, uint8_t value) {
        if (!CheckBytes(insnVA, expected, expectedLen)) {
            ++gPatchSkippedCount;
            Log("%s skipped: byte guard failed at 0x%08lX.", name, static_cast<unsigned long>(insnVA));
            return false;
        }
        if (!WriteMemory(reinterpret_cast<void*>(immVA), &value, sizeof(value))) {
            ++gPatchFailedCount;
            Log("%s failed: could not write imm8 at 0x%08lX.", name, static_cast<unsigned long>(immVA));
            return false;
        }
        ++gPatchAppliedCount;
        Log("%s -> %u", name, static_cast<unsigned>(value));
        return true;
    }

    static bool PatchImm32(const char* name, uintptr_t insnVA, const uint8_t* expected, size_t expectedLen, uintptr_t immVA, uint32_t value) {
        if (!CheckBytes(insnVA, expected, expectedLen)) {
            ++gPatchSkippedCount;
            Log("%s skipped: byte guard failed at 0x%08lX.", name, static_cast<unsigned long>(insnVA));
            return false;
        }
        if (!WriteMemory(reinterpret_cast<void*>(immVA), &value, sizeof(value))) {
            ++gPatchFailedCount;
            Log("%s failed: could not write imm32 at 0x%08lX.", name, static_cast<unsigned long>(immVA));
            return false;
        }
        ++gPatchAppliedCount;
        Log("%s imm32 -> 0x%08lX", name, static_cast<unsigned long>(value));
        return true;
    }

    static bool PatchRangeJmp(const char* name, uintptr_t va, const uint8_t* expected, size_t expectedLen, void* target) {
        if (expectedLen < 5) { ++gPatchFailedCount; Log("%s failed: range too small for JMP.", name); return false; }
        if (!CheckBytes(va, expected, expectedLen)) { ++gPatchSkippedCount; Log("%s skipped: byte guard failed at 0x%08lX.", name, static_cast<unsigned long>(va)); return false; }
        uint8_t patch[16]{};
        if (expectedLen > sizeof(patch)) { ++gPatchFailedCount; Log("%s failed: patch range too long.", name); return false; }
        patch[0] = 0xE9;
        const intptr_t rel = reinterpret_cast<uintptr_t>(target) - (va + 5);
        const int32_t rel32 = static_cast<int32_t>(rel);
        std::memcpy(&patch[1], &rel32, sizeof(rel32));
        for (size_t i = 5; i < expectedLen; ++i) patch[i] = 0x90;
        if (!WriteMemory(reinterpret_cast<void*>(va), patch, expectedLen)) { ++gPatchFailedCount; Log("%s failed: could not write JMP at 0x%08lX.", name, static_cast<unsigned long>(va)); return false; }
        ++gPatchAppliedCount;
        Log("%s JMP applied.", name);
        return true;
    }


    static void* CallThiscallNoArg(void* obj, unsigned vtableOffset) {
        if (!obj || IsBadReadPtr(obj, sizeof(void*))) return nullptr;
        void** vtable = *reinterpret_cast<void***>(obj);
        if (!vtable || IsBadReadPtr(vtable, vtableOffset + sizeof(void*))) return nullptr;
        void* fn = vtable[vtableOffset / sizeof(void*)];
        if (!fn || IsBadCodePtr(reinterpret_cast<FARPROC>(fn))) return nullptr;
        using Fn = void* (__fastcall*)(void*, void*);
        return reinterpret_cast<Fn>(fn)(obj, nullptr);
    }

    static bool InstallChopperSpeedThisHook() {
        if (gChopperSpeedHookInstalled) return true;

        if (!CheckBytes(kOnDrivingHookVA, kOnDrivingHookGuard, sizeof(kOnDrivingHookGuard))) {
            ++gPatchSkippedCount;
            Log("Chopper::Speed hook skipped: AIVehicleHelicopter::OnDriving prologue guard failed at 0x%08lX. Another ASI may already hook it.",
                static_cast<unsigned long>(kOnDrivingHookVA));
            return false;
        }

        gOnDrivingTrampoline = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!gOnDrivingTrampoline) {
            ++gPatchFailedCount;
            Log("Chopper::Speed hook failed: trampoline allocation failed.");
            return false;
        }

        uint8_t* p = gOnDrivingTrampoline;
        // mov [gCapturedHeliThis], ecx ; capture AIVehicleHelicopter* before running original prologue
        *p++ = 0x89;
        *p++ = 0x0D;
        *reinterpret_cast<uint32_t*>(p) = reinterpret_cast<uint32_t>(&gCapturedHeliThis);
        p += 4;
        std::memcpy(p, reinterpret_cast<void*>(kOnDrivingHookVA), 5);
        p += 5;
        *p++ = 0xE9;
        *reinterpret_cast<int32_t*>(p) = static_cast<int32_t>((kOnDrivingHookVA + 5) - (reinterpret_cast<uintptr_t>(p) + 4));
        p += 4;

        uint8_t patch[5]{};
        patch[0] = 0xE9;
        *reinterpret_cast<int32_t*>(&patch[1]) = static_cast<int32_t>(reinterpret_cast<uintptr_t>(gOnDrivingTrampoline) - (kOnDrivingHookVA + 5));
        if (!WriteMemory(reinterpret_cast<void*>(kOnDrivingHookVA), patch, sizeof(patch))) {
            ++gPatchFailedCount;
            Log("Chopper::Speed hook failed: could not patch OnDriving prologue.");
            return false;
        }

        gChopperSpeedHookInstalled = true;
        ++gPatchAppliedCount;
        Log("Chopper::Speed hook installed at 0x%08lX trampoline=0x%08lX.",
            static_cast<unsigned long>(kOnDrivingHookVA),
            static_cast<unsigned long>(reinterpret_cast<uintptr_t>(gOnDrivingTrampoline)));
        return true;
    }

    static void LogChopperSpeedThrottled(const char* fmt, ...) {
        DWORD now = GetTickCount();
        if (now - gLastChopperSpeedLogTick < 1000) return;
        gLastChopperSpeedLogTick = now;

        char buf[1024]{};
        va_list args;
        va_start(args, fmt);
        std::vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        Log("%s", buf);
    }

    static void ChopperSpeedTick() {
        void* globalHeli = nullptr;
        __try {
            globalHeli = *reinterpret_cast<void**>(kGlobalHeliVehicleVA);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }
        if (!globalHeli) {
            LogChopperSpeedThrottled("[Chopper::Speed] no active global heli.");
            return;
        }

        void* heli = const_cast<void*>(gCapturedHeliThis);
        if (!heli || IsBadReadPtr(heli, 0x90)) {
            LogChopperSpeedThrottled("[Chopper::Speed] heli-this not captured yet (%p).", heli);
            return;
        }

        float desired = 0.0f;
        void* owner = nullptr;
        __try {
            desired = *reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(heli) + kHeliDriveSpeedOffset);
            owner = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(heli) + kHeliOwnerOffset);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return;
        }

        if (gCfg.ChopperIgnoreNegativeDesiredSpeed && desired < 0.0f) {
            LogChopperSpeedThrottled("[Chopper::Speed] skip negative mDriveSpeed=%.1f.", desired);
            return;
        }
        if (desired < 0.0f) desired = 0.0f;

        void* rb = CallThiscallNoArg(owner, kOwnerGetRigidBodyVTableOffset);
        if (!rb) return;
        float* vel = reinterpret_cast<float*>(CallThiscallNoArg(rb, kRigidBodyGetLinearVelocityVTableOffset));
        if (!vel || IsBadWritePtr(vel, sizeof(float) * 3)) return;

        const float vx = vel[0];
        const float vz = vel[2];
        const float currentSpeed = static_cast<float>(std::sqrt(static_cast<double>(vx * vx + vz * vz)));
        if (currentSpeed < 2.0f) return;

        float target = desired * gCfg.ChopperSpeedFactor;
        if (gCfg.ChopperMaxSpeed > 0.0f && target > gCfg.ChopperMaxSpeed) target = gCfg.ChopperMaxSpeed;

        // Only fully return when both target and current speed are below the low-speed floor.
        // If currentSpeed is high but target drops low, let OverspeedBrake help bleed speed off.
        if (target < gCfg.ChopperMinApplySpeed && currentSpeed < gCfg.ChopperMinApplySpeed) return;

        static float prevDirX = 0.0f;
        static float prevDirZ = 0.0f;
        static float turnAmount = 0.0f;

        const float dirX = vx / currentSpeed;
        const float dirZ = vz / currentSpeed;
        if (prevDirX != 0.0f || prevDirZ != 0.0f) {
            float cross = prevDirX * dirZ - prevDirZ * dirX;
            if (cross < 0.0f) cross = -cross;
            turnAmount = turnAmount * 0.92f + cross * 0.08f;
        }
        prevDirX = dirX;
        prevDirZ = dirZ;

        float turnScale = 1.0f;
        if (gCfg.ChopperTurnSlowdown) {
            turnScale = 1.0f / (1.0f + turnAmount * gCfg.ChopperTurnSlowdownStrength);
            if (turnScale < 0.25f) turnScale = 0.25f;
            if (turnScale > 1.0f) turnScale = 1.0f;
        }

        const float adjustedTarget = target * turnScale;
        const float delta = adjustedTarget - currentSpeed;
        float rate = (delta >= 0.0f) ? gCfg.ChopperSpeedPush : gCfg.ChopperOverspeedBrake;
        if (rate <= 0.0f) return;
        if (rate > 1.0f) rate = 1.0f;

        float newSpeed = currentSpeed + delta * rate;
        if (gCfg.ChopperMaxSpeed > 0.0f && newSpeed > gCfg.ChopperMaxSpeed) newSpeed = gCfg.ChopperMaxSpeed;
        if (newSpeed < 0.0f) newSpeed = 0.0f;

        const float scale = newSpeed / currentSpeed;
        vel[0] = vx * scale;
        vel[2] = vz * scale;

        LogChopperSpeedThrottled("[Chopper::Speed] mDriveSpeed=%.1f cur=%.1f target=%.1f adjusted=%.1f new=%.1f turn=%.3f x%.2f rate=%.3f.",
            desired, currentSpeed, target, adjustedTarget, newSpeed, turnAmount, turnScale, rate);
    }

    static DWORD WINAPI ChopperSpeedThread(void*) {
        Log("Chopper::Speed worker thread started.");
        for (;;) {
            if (gCfg.EnableChopperSpeed) ChopperSpeedTick();
            Sleep(8);
        }
    }

    static void ApplyChopperSpeedPatches() {
        if (!gCfg.EnableChopperSpeed) {
            Log("Chopper::Speed disabled.");
            return;
        }
        if (!InstallChopperSpeedThisHook()) return;
        if (!gChopperSpeedThreadStarted) {
            HANDLE thread = CreateThread(nullptr, 0, ChopperSpeedThread, nullptr, 0, nullptr);
            if (thread) {
                CloseHandle(thread);
                gChopperSpeedThreadStarted = true;
                Log("Chopper::Speed enabled: Factor=%.3f MaxSpeed=%.3f Push=%.3f OverspeedBrake=%.3f MinApply=%.3f TurnSlowdown=%d Strength=%.3f IgnoreNegative=%d.",
                    gCfg.ChopperSpeedFactor, gCfg.ChopperMaxSpeed, gCfg.ChopperSpeedPush,
                    gCfg.ChopperOverspeedBrake, gCfg.ChopperMinApplySpeed,
                    gCfg.ChopperTurnSlowdown ? 1 : 0, gCfg.ChopperTurnSlowdownStrength,
                    gCfg.ChopperIgnoreNegativeDesiredSpeed ? 1 : 0);
            }
            else {
                ++gPatchFailedCount;
                Log("Chopper::Speed failed: could not create worker thread.");
            }
        }
    }

    static void ApplyVisionPatches() {
        if (!gCfg.EnableVisionPatches) {
            Log("Chopper::Vision disabled.");
            return;
        }
        if (!gCfg.HeliSeesThroughWalls) {
            Log("Chopper::Vision enabled, but HeliSeesThroughWalls=0; no branch patch applied.");
            return;
        }

        static const uint8_t kVisionSightGuard[4] = { 0x84, 0xC0, 0x75, 0x1C };
        PatchImm8("Chopper::Vision force IsPerpInSight success / see-through-walls branch",
            0x00427872u,
            kVisionSightGuard,
            sizeof(kVisionSightGuard),
            0x00427874u,
            0xEB);
    }

    static void WriteRuntimeFloat(uintptr_t va, float value) { WriteMemory(reinterpret_cast<void*>(va), &value, sizeof(value)); }

    static bool QueryPlayerIsRacing() {
        if (!gCfg.EnableRaceBasedProfiles || gPlayerPerpVehicle == 0) return false;
        using IsRacingFn = bool(__thiscall*)(uintptr_t);
        const auto IsRacing = reinterpret_cast<IsRacingFn>(0x00409500u);
        return IsRacing(gPlayerPerpVehicle);
    }

    static void ApplyHeatProfile(uint32_t heatLevel, const char* reason) {
        if (!gCfg.EnableHeatBasedProfiles) return;
        if (heatLevel < 1 || heatLevel > 10) return;

        HeatProfile& p = (gCfg.EnableRaceBasedProfiles && gCurrentIsRacing) ? gRaceProfiles[heatLevel] : gHeatProfiles[heatLevel];
        if (!p.enabled) {
            if (gCfg.LogHeatProfileChanges) Log("%s profile %lu disabled; keeping previous values.", (gCurrentIsRacing ? "Race" : "Heat"), static_cast<unsigned long>(heatLevel));
            return;
        }

        gCurrentHeatLevel = heatLevel;

        gCfg.SkidCooldownThreshold = p.SkidCooldownThreshold;
        gCfg.SkidEntryMaxDistance = p.SkidEntryMaxDistance;
        gCfg.SkidEntryMinDistance = p.SkidEntryMinDistance;
        gCfg.SkidEntryDotRequirement = p.SkidEntryDotRequirement;
        gCfg.SkidEntryMaxHeightDelta = p.SkidEntryMaxHeightDelta;

        gCfg.StraightLeadSpeedScale = p.StraightLeadSpeedScale;
        gCfg.StraightLeadBase = p.StraightLeadBase;
        gCfg.StraightLeadMax = p.StraightLeadMax;
        gCfg.StraightLeadSkidMultiplier = p.StraightLeadSkidMultiplier;
        gCfg.StraightHeightPositiveTimer = p.StraightHeightPositiveTimer;
        gCfg.StraightHeightClose = p.StraightHeightClose;
        gCfg.StraightHeightHigh = p.StraightHeightHigh;

        gCfg.SkidSideOffset = p.SkidSideOffset;
        gCfg.SkidApproachVelocityLead = p.SkidApproachVelocityLead;
        gCfg.SkidApproachHeight = p.SkidApproachHeight;
        gCfg.SkidLowExtraHeight = p.SkidLowExtraHeight;
        gCfg.SkidStrikeStartDistance = p.SkidStrikeStartDistance;
        gCfg.SkidStrikeRightDotRequirement = p.SkidStrikeRightDotRequirement;
        gCfg.SkidStrikeVelocityLead = p.SkidStrikeVelocityLead;
        gCfg.SkidStrikeBackScale = p.SkidStrikeBackScale;
        gCfg.SkidAbortDistanceAheadSq = p.SkidAbortDistanceAheadSq;
        gCfg.SkidAbortDistanceBehindSq = p.SkidAbortDistanceBehindSq;

        gCfg.DestinationVelocityMultiplier = p.DestinationVelocityMultiplier;
        gCfg.DestinationVelocityBlend = p.DestinationVelocityBlend;
        gCfg.MaxChopperAccel = p.MaxChopperAccel;
        gCfg.MinChopperAccel = p.MinChopperAccel;
        gCfg.ChopperSpeedLimitRatio = p.ChopperSpeedLimitRatio;
        gCfg.OnDrivingDecelSpeedScale = p.OnDrivingDecelSpeedScale;
        gCfg.OnDrivingTurnResponseScale = p.OnDrivingTurnResponseScale;
        gCfg.OnDrivingTurnClamp = p.OnDrivingTurnClamp;
        gCfg.OnDrivingSmoothingOldWeight = p.OnDrivingSmoothingOldWeight;
        gCfg.OnDrivingSmoothingFinalScale = p.OnDrivingSmoothingFinalScale;

        gCfg.ExitFlySpeed = p.ExitFlySpeed;
        gCfg.ExitSeekUpThreshold = p.ExitSeekUpThreshold;
        gCfg.ExitSeekAheadDistance = p.ExitSeekAheadDistance;
        gCfg.ExitSeekCarReachDistanceSq = p.ExitSeekCarReachDistanceSq;
        gCfg.ExitRightNegativeScale = p.ExitRightNegativeScale;
        gCfg.ExitFlyoutBackScale = p.ExitFlyoutBackScale;
        gCfg.ExitFlyoutExtraHeight = p.ExitFlyoutExtraHeight;
        gCfg.ExitFinishHeight = p.ExitFinishHeight;
        gCfg.ExitFinishDistanceSq = p.ExitFinishDistanceSq;
        gCfg.ExitDoDrivingMode = p.ExitDoDrivingMode;

        fSkidCooldownThreshold = p.SkidCooldownThreshold;
        fSkidEntryMaxDistance = p.SkidEntryMaxDistance;
        fSkidEntryMinDistance = p.SkidEntryMinDistance;
        fSkidEntryDotRequirement = p.SkidEntryDotRequirement;
        fSkidEntryMaxHeightDelta = p.SkidEntryMaxHeightDelta;

        fStraightLeadSpeedScale = p.StraightLeadSpeedScale;
        fStraightLeadBase = p.StraightLeadBase;
        fStraightLeadMax = p.StraightLeadMax;
        fStraightLeadSkidMultiplier = p.StraightLeadSkidMultiplier;
        fStraightHeightPositiveTimer = p.StraightHeightPositiveTimer;
        fStraightHeightClose = p.StraightHeightClose;
        fStraightHeightHigh = p.StraightHeightHigh;

        fSkidSideOffsetPositive = p.SkidSideOffset;
        fSkidSideOffsetNegative = -p.SkidSideOffset;
        fSkidApproachVelocityLead = p.SkidApproachVelocityLead;
        fSkidApproachHeight = p.SkidApproachHeight;
        fSkidLowExtraHeight = p.SkidLowExtraHeight;
        fSkidStrikeStartDistance = p.SkidStrikeStartDistance;
        fSkidStrikeRightDotRequirement = p.SkidStrikeRightDotRequirement;
        fSkidStrikeVelocityLead = p.SkidStrikeVelocityLead;
        fSkidStrikeBackScale = p.SkidStrikeBackScale;
        fSkidAbortDistanceAheadSq = p.SkidAbortDistanceAheadSq;
        fSkidAbortDistanceBehindSq = p.SkidAbortDistanceBehindSq;

        fDestinationVelocityMultiplier = p.DestinationVelocityMultiplier;
        fDestinationVelocityBlend = p.DestinationVelocityBlend;
        fOnDrivingDecelSpeedScale = p.OnDrivingDecelSpeedScale;
        fOnDrivingTurnResponseScale = p.OnDrivingTurnResponseScale;
        fOnDrivingTurnClampPositive = p.OnDrivingTurnClamp;
        fOnDrivingTurnClampNegative = -p.OnDrivingTurnClamp;
        fOnDrivingSmoothingOldWeight = p.OnDrivingSmoothingOldWeight;
        fOnDrivingSmoothingFinalScale = p.OnDrivingSmoothingFinalScale;

        fExitFlySpeed = p.ExitFlySpeed;
        fExitSeekUpThreshold = p.ExitSeekUpThreshold;
        fExitSeekAheadDistance = p.ExitSeekAheadDistance;
        fExitSeekCarReachDistanceSq = p.ExitSeekCarReachDistanceSq;
        fExitRightNegativeScale = p.ExitRightNegativeScale;
        fExitFlyoutBackScale = p.ExitFlyoutBackScale;
        fExitFlyoutExtraHeight = p.ExitFlyoutExtraHeight;
        fExitFinishHeight = p.ExitFinishHeight;
        fExitFinishDistanceSq = p.ExitFinishDistanceSq;

        if (gCfg.EnableAIVehicleOnDrivingDataPatches) {
            WriteRuntimeFloat(0x008F8DCCu, p.MaxChopperAccel);
            WriteRuntimeFloat(0x008F8DD0u, p.MinChopperAccel);
            WriteRuntimeFloat(0x008F8DD4u, p.ChopperSpeedLimitRatio);
        }

        // These two patches are immediate values, not redirected f-memory reads, so update them live too.
        if (gCfg.EnableHeliExitActionPatches && gCfg.PatchExitFlySpeed) {
            const uint32_t flySpeedBits = FloatBits(p.ExitFlySpeed);
            WriteMemory(reinterpret_cast<void*>(0x00427BFFu), &flySpeedBits, sizeof(flySpeedBits));
        }
        if (gCfg.EnableHeliExitActionPatches && gCfg.PatchExitDoDrivingMode) {
            WriteMemory(reinterpret_cast<void*>(0x00427C29u), &p.ExitDoDrivingMode, sizeof(p.ExitDoDrivingMode));
        }

        if (gCfg.LogHeatProfileChanges) {
            Log("%s profile applied: level=%lu reason=%s turn=%.3f clamp=%.3f destMult=%.3f maxAccel=%.3f ratio=%.3f skidCd=%.3f exitSpeed=%.3f mode=%u",
                (gCurrentIsRacing ? "Race" : "Heat"), static_cast<unsigned long>(heatLevel), reason ? reason : "unknown",
                p.OnDrivingTurnResponseScale, p.OnDrivingTurnClamp, p.DestinationVelocityMultiplier,
                p.MaxChopperAccel, p.ChopperSpeedLimitRatio, p.SkidCooldownThreshold, p.ExitFlySpeed,
                static_cast<unsigned int>(p.ExitDoDrivingMode));
        }
    }

    static void __cdecl OnHeatLevelObserved(uint32_t heatLevel, uintptr_t perpVehicle) {
        if (!gCfg.EnableHeatBasedProfiles) return;
        if (heatLevel < 1 || heatLevel > 10) return;
        if (gPlayerPerpVehicle != 0 && perpVehicle != gPlayerPerpVehicle) return;

        const bool isRacing = QueryPlayerIsRacing();
        if (heatLevel == gCurrentHeatLevel && isRacing == gCurrentIsRacing) return;

        gCurrentIsRacing = isRacing;
        ApplyHeatProfile(heatLevel, isRacing ? "race/heat observer" : "roam/heat observer");
    }

    extern "C" void __cdecl CHA_OnHeatLevelObserved(uint32_t heatLevel, uintptr_t perpVehicle) {
        OnHeatLevelObserved(heatLevel, perpVehicle);
    }

    extern "C" void __cdecl CHA_SetPlayerPerpVehicle(uintptr_t playerPerpVehicle) {
        if (gPlayerPerpVehicle == 0) {
            gPlayerPerpVehicle = playerPerpVehicle;
        }
    }

    static constexpr uintptr_t kHeatLevelObserverEntrance = 0x004090BEu;
    static constexpr uintptr_t kHeatLevelObserverExit = 0x004090C6u;
    static constexpr uintptr_t kPlayerConstructorEntrance = 0x0043F005u;
    static constexpr uintptr_t kPlayerConstructorExit = 0x0043F00Fu;

#if defined(__GNUC__) && !defined(_MSC_VER)
    // MinGW/GCC uses AT&T asm by default and does not support MSVC __asm { } blocks.
    // Keep the stubs naked, but write the trampoline in GCC inline asm.
    __attribute__((naked)) void HeatLevelObserverHook() {
        __asm__ __volatile__(
            ".intel_syntax noprefix\n"
            "pushad\n"
            "push esi\n"
            "push ebp\n"
            "call _CHA_OnHeatLevelObserved\n"
            "add esp, 8\n"
            "popad\n"
            "cmp bl, byte ptr [esi + 0x2E]\n"
            "setne al\n"
            "cmp ebp, edi\n"
            "push 0x004090C6\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }

    __attribute__((naked)) void PlayerConstructorHook() {
        __asm__ __volatile__(
            ".intel_syntax noprefix\n"
            "lea eax, dword ptr [esi + 0x758]\n"
            "mov dword ptr [eax], 0x00892988\n"
            "pushad\n"
            "push eax\n"
            "call _CHA_SetPlayerPerpVehicle\n"
            "add esp, 4\n"
            "popad\n"
            "push 0x0043F00F\n"
            "ret\n"
            ".att_syntax prefix\n"
        );
    }
#else
    __declspec(naked) void HeatLevelObserverHook() {
        __asm {
            // Do not early-out on heat only; race status can change while heat stays the same.
            pushad
            push esi
            push ebp
            call CHA_OnHeatLevelObserved
            add esp, 8
            popad
            cmp bl, byte ptr[esi + 0x2E]
            setne al
            cmp ebp, edi
            push 004090C6h
            ret
        }
    }

    __declspec(naked) void PlayerConstructorHook() {
        __asm {
            lea eax, dword ptr[esi + 0x758]
            mov dword ptr[eax], 00892988h
            pushad
            push eax
            call CHA_SetPlayerPerpVehicle
            add esp, 4
            popad
            push 0043F00Fh
            ret
        }
    }
#endif

    static const uint8_t kHeatLevelObserverGuard[] = { 0x3A,0x5E,0x2E,0x0F,0x95,0xC0,0x3B,0xEF };
    static const uint8_t kPlayerConstructorGuard[] = { 0xC7,0x86,0x58,0x07,0x00,0x00,0x88,0x29,0x89,0x00 };

    static void ApplyHeatObserverPatches() {
        if (!gCfg.EnableHeatBasedProfiles || !gCfg.InstallHeatObserverHook) return;
        PatchRangeJmp("Heat profile observer hook", kHeatLevelObserverEntrance, kHeatLevelObserverGuard, sizeof(kHeatLevelObserverGuard), reinterpret_cast<void*>(HeatLevelObserverHook));
        PatchRangeJmp("Player perp-vehicle tracker hook", kPlayerConstructorEntrance, kPlayerConstructorGuard, sizeof(kPlayerConstructorGuard), reinterpret_cast<void*>(PlayerConstructorHook));
    }

    static uint32_t FloatBits(float value) {
        uint32_t out = 0;
        std::memcpy(&out, &value, sizeof(out));
        return out;
    }

    static bool WriteBoolData(const char* name, uintptr_t va, uint8_t value) {
        if (!WriteMemory(reinterpret_cast<void*>(va), &value, sizeof(value))) {
            ++gPatchFailedCount;
            Log("%s failed: could not write bool at 0x%08lX.", name, static_cast<unsigned long>(va));
            return false;
        }
        ++gPatchAppliedCount;
        Log("%s data -> %u", name, static_cast<unsigned>(value));
        return true;
    }

    static float AbsF(float v) { return v < 0.0f ? -v : v; }

    static bool WriteFloatDataGuarded(const char* name, uintptr_t va, float expected, float value) {
        const float current = *reinterpret_cast<const float*>(va);
        if (AbsF(current - expected) > 0.001f && AbsF(current - value) > 0.001f) {
            ++gPatchSkippedCount;
            Log("%s skipped: data guard failed at 0x%08lX, current %.6f expected %.6f.",
                name, static_cast<unsigned long>(va), current, expected);
            return false;
        }
        if (!WriteMemory(reinterpret_cast<void*>(va), &value, sizeof(value))) {
            ++gPatchFailedCount;
            Log("%s failed: could not write float at 0x%08lX.", name, static_cast<unsigned long>(va));
            return false;
        }
        ++gPatchAppliedCount;
        Log("%s data %.3f -> %.3f", name, current, value);
        return true;
    }

#define BYTES(name, ...) static const uint8_t name[] = { __VA_ARGS__ }

    BYTES(kForceSkidAttrGuard, 0x74, 0x14);
    BYTES(kNop2, 0x90, 0x90);
    BYTES(kNop6, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90);

    BYTES(kSkidTimerCmpGuard, 0xD8, 0x1D, 0xB8, 0x0D, 0x89, 0x00);
    BYTES(kEntryMaxDistGuard, 0xD8, 0x1D, 0x5C, 0x10, 0x89, 0x00);
    BYTES(kEntryMinDistGuard, 0xD8, 0x1D, 0xA4, 0x0D, 0x89, 0x00);
    BYTES(kEntryDotGuard, 0xD8, 0x1D, 0x58, 0x10, 0x89, 0x00);
    BYTES(kEntryYDeltaGuard, 0xD8, 0x1D, 0x48, 0x06, 0x89, 0x00);
    BYTES(kLeadSpeedScaleGuard, 0xD8, 0x0D, 0x54, 0x10, 0x89, 0x00);
    BYTES(kLeadBaseGuard, 0xD8, 0x05, 0x14, 0x06, 0x89, 0x00);
    BYTES(kLeadMaxGuard, 0xD8, 0x15, 0x58, 0x06, 0x89, 0x00);
    BYTES(kLeadSkidMultGuard, 0xD8, 0x0D, 0x50, 0x10, 0x89, 0x00);
    BYTES(kHeightPosGuard, 0xD8, 0x05, 0x3C, 0x0D, 0x89, 0x00);
    BYTES(kHeightCloseGuard, 0xD8, 0x05, 0x48, 0x10, 0x89, 0x00);
    BYTES(kHeightHighGuard, 0xD8, 0x05, 0x44, 0x10, 0x89, 0x00);
    BYTES(kSkidSidePosGuard, 0xD9, 0x05, 0x48, 0x10, 0x89, 0x00);
    BYTES(kSkidSideNegGuard, 0xD9, 0x05, 0x78, 0x10, 0x89, 0x00);
    BYTES(kSkidApproachVelGuard, 0xD8, 0x0D, 0x4C, 0x06, 0x89, 0x00);
    BYTES(kSkidApproachHeightGuard, 0xD8, 0x05, 0x74, 0x10, 0x89, 0x00);
    BYTES(kSkidLowExtraGuard, 0xD8, 0x05, 0x04, 0x06, 0x89, 0x00);
    BYTES(kStrikeDistGuard, 0xD8, 0x1D, 0x98, 0x0E, 0x89, 0x00);
    BYTES(kStrikeDotGuard, 0xD8, 0x1D, 0x70, 0x10, 0x89, 0x00);
    BYTES(kStrikeVelGuard, 0xD8, 0x0D, 0x6C, 0x10, 0x89, 0x00);
    BYTES(kStrikeBackGuard, 0xD8, 0x0D, 0x68, 0x10, 0x89, 0x00);
    BYTES(kAbortAheadGuard, 0xD8, 0x1D, 0x64, 0x10, 0x89, 0x00);
    BYTES(kAbortBehindGuard, 0xD8, 0x1D, 0x60, 0x10, 0x89, 0x00);
    BYTES(kDestVelMultGuard, 0xD8, 0x0D, 0x98, 0x0E, 0x89, 0x00);
    BYTES(kDestVelBlendGuard, 0xD8, 0x0D, 0x74, 0x50, 0x89, 0x00);
    BYTES(kOnDriveDecelSpeedScaleGuard, 0xD8, 0x0D, 0x44, 0xAE, 0x8A, 0x00);
    BYTES(kOnDriveTurnScaleGuard, 0xD8, 0x0D, 0xEC, 0xB8, 0x8A, 0x00);
    BYTES(kOnDriveTurnClampPosCmpGuard, 0xD8, 0x1D, 0x5C, 0xAE, 0x8A, 0x00);
    BYTES(kOnDriveTurnClampPosLoadGuard, 0xD9, 0x05, 0x5C, 0xAE, 0x8A, 0x00);
    BYTES(kOnDriveTurnClampNegLoadGuard, 0xD9, 0x05, 0xE8, 0xB8, 0x8A, 0x00);
    BYTES(kOnDriveSmoothingOldWeightGuard, 0xD8, 0x0D, 0x18, 0x07, 0x8A, 0x00);
    BYTES(kOnDriveSmoothingFinalScaleGuard, 0xD8, 0x0D, 0x14, 0x0F, 0x89, 0x00);
    BYTES(kForceOnDriveMaxSpeedCapGuard, 0x74, 0x0A);
    BYTES(kHeliSheetBaseWriteGuard, 0xC6, 0x05, 0x21, 0xD6, 0x90, 0x00, 0x00);
    BYTES(kFuelExitBranchGuard, 0x7A, 0x4C);
    BYTES(kExitHeightCompareGuard, 0xD8, 0x1D, 0xF8, 0xB1, 0x8E, 0x00);
    BYTES(kExitFinishDistanceSqGuard, 0xD8, 0x1D, 0x0C, 0x16, 0x89, 0x00);
    BYTES(kExitSeekUpThresholdGuard, 0xD8, 0x1D, 0xAC, 0x0D, 0x89, 0x00);
    BYTES(kExitSeekAheadGuard, 0xD8, 0x0D, 0x10, 0x16, 0x89, 0x00);
    BYTES(kExitSeekHeightGuard, 0xD9, 0x05, 0xF8, 0xB1, 0x8E, 0x00);
    BYTES(kExitReachDistanceSqGuard, 0xD8, 0x1D, 0xC0, 0x0D, 0x89, 0x00);
    BYTES(kExitRightNegativeScaleGuard, 0xD9, 0x05, 0x18, 0x16, 0x89, 0x00);
    BYTES(kExitFlyoutBackScaleGuard, 0xD8, 0x0D, 0x14, 0x16, 0x89, 0x00);
    BYTES(kExitFlyoutExtraHeightGuard, 0xD8, 0x05, 0xA4, 0x0D, 0x89, 0x00);
    BYTES(kExitFlySpeedPushGuard, 0x68, 0x00, 0x00, 0xC8, 0x42);
    BYTES(kExitDoDrivingPushGuard, 0x6A, 0x07);
    BYTES(kLegacyRenderGuard, 0xD8, 0x1D, 0x64, 0xAC, 0x8A, 0x00);
    BYTES(kHeliRenderForceServiceInViewGuard,
        0x8B, 0x86, 0xB0, 0x01, 0x00, 0x00, 0x39, 0x86, 0xB4, 0x01, 0x00, 0x00,
        0x72, 0x08, 0x85, 0xC0, 0x74, 0x04, 0xB0, 0x01, 0xEB, 0x02, 0x32, 0xC0);
    BYTES(kHeliRenderServiceDistanceGuard, 0xD9, 0x86, 0xB8, 0x01, 0x00, 0x00);
    BYTES(kHeliRenderServiceFailHideGuard, 0xC6, 0x46, 0x60, 0x01);
    BYTES(kHeliRenderResetDistanceGuard, 0xC7, 0x86, 0xB8, 0x01, 0x00, 0x00, 0xF0, 0x23, 0x74, 0x49);
    BYTES(kRoadblockGoalBranchGuard, 0x75, 0x25);
    BYTES(kHeliWashPhysicsStrengthPushGuard, 0x68, 0x00, 0x00, 0x80, 0x3F);

    // Dispatch/spawn layer byte guards from the copheli / CHOPPER / ReqHeliJoin xrefs.
    BYTES(kCopheliWeightedSelectionGateGuard, 0x75, 0x0E);                    // 0x0042BC54: JNE skip copheli gate
    BYTES(kSelectorExistingHeliGateGuard, 0x75, 0x4C);                       // 0x0042BB04: JNE skip selector-trigger heli request
    BYTES(kSpecialSpawnExistingHeliGateGuard, 0x0F, 0x85, 0x1B, 0x02, 0x00, 0x00); // 0x004269CA: JNE fail if heli-involved global is set
    BYTES(kForceSelectorReturnCopheliGuard, 0x74, 0x17);                     // 0x0042BB5A: JE normal weighted selector
    BYTES(kSpawnCapImmediateRequestGuard, 0x7D, 0x51);                       // 0x0043EB90: JGE skip immediate string create path

    static void ApplySkidLogicPatches() {
        if (gCfg.ForceSkidHitAttribute) {
            PatchBytes("Force SkidHitEnabled branch", 0x00412805u, kForceSkidAttrGuard, sizeof(kForceSkidAttrGuard), kNop2, sizeof(kNop2));
        }

        if (gCfg.EnableSkidEntryGatePatches) {
            const FloatOperandPatch patches[] = {
                {"Skid cooldown entry threshold", 0x0041280Au, 0x0041280Cu, kSkidTimerCmpGuard, sizeof(kSkidTimerCmpGuard), &fSkidCooldownThreshold},
                {"Skid cooldown height threshold", 0x004129C3u, 0x004129C5u, kSkidTimerCmpGuard, sizeof(kSkidTimerCmpGuard), &fSkidCooldownThreshold},
                {"Skid entry max distance", 0x00412849u, 0x0041284Bu, kEntryMaxDistGuard, sizeof(kEntryMaxDistGuard), &fSkidEntryMaxDistance},
                {"Skid entry min distance", 0x0041285Eu, 0x00412860u, kEntryMinDistGuard, sizeof(kEntryMinDistGuard), &fSkidEntryMinDistance},
                {"Skid entry dot requirement", 0x00412885u, 0x00412887u, kEntryDotGuard, sizeof(kEntryDotGuard), &fSkidEntryDotRequirement},
                {"Skid entry max height delta", 0x004128A5u, 0x004128A7u, kEntryYDeltaGuard, sizeof(kEntryYDeltaGuard), &fSkidEntryMaxHeightDelta},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }

        if (gCfg.EnableLeadAndHeightPatches) {
            const FloatOperandPatch patches[] = {
                {"Straight lead speed scale", 0x0041293Du, 0x0041293Fu, kLeadSpeedScaleGuard, sizeof(kLeadSpeedScaleGuard), &fStraightLeadSpeedScale},
                {"Straight lead base", 0x00412946u, 0x00412948u, kLeadBaseGuard, sizeof(kLeadBaseGuard), &fStraightLeadBase},
                {"Straight lead max", 0x0041294Cu, 0x0041294Eu, kLeadMaxGuard, sizeof(kLeadMaxGuard), &fStraightLeadMax},
                {"Straight lead skid multiplier", 0x00412965u, 0x00412967u, kLeadSkidMultGuard, sizeof(kLeadSkidMultGuard), &fStraightLeadSkidMultiplier},
                {"Straight positive-timer height", 0x004129A7u, 0x004129A9u, kHeightPosGuard, sizeof(kHeightPosGuard), &fStraightHeightPositiveTimer},
                {"Straight close height", 0x004129D4u, 0x004129D6u, kHeightCloseGuard, sizeof(kHeightCloseGuard), &fStraightHeightClose},
                {"Straight high height", 0x004129E0u, 0x004129E2u, kHeightHighGuard, sizeof(kHeightHighGuard), &fStraightHeightHigh},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }

        if (gCfg.EnableSkidStrikePatches) {
            const FloatOperandPatch patches[] = {
                {"Skid side offset positive", 0x00412C3Eu, 0x00412C40u, kSkidSidePosGuard, sizeof(kSkidSidePosGuard), &fSkidSideOffsetPositive},
                {"Skid side offset negative", 0x00412C57u, 0x00412C59u, kSkidSideNegGuard, sizeof(kSkidSideNegGuard), &fSkidSideOffsetNegative},
                {"Skid approach velocity lead X", 0x00412C87u, 0x00412C89u, kSkidApproachVelGuard, sizeof(kSkidApproachVelGuard), &fSkidApproachVelocityLead},
                {"Skid approach velocity lead Y", 0x00412C97u, 0x00412C99u, kSkidApproachVelGuard, sizeof(kSkidApproachVelGuard), &fSkidApproachVelocityLead},
                {"Skid approach velocity lead Z", 0x00412CA7u, 0x00412CA9u, kSkidApproachVelGuard, sizeof(kSkidApproachVelGuard), &fSkidApproachVelocityLead},
                {"Skid approach height", 0x00412CB7u, 0x00412CB9u, kSkidApproachHeightGuard, sizeof(kSkidApproachHeightGuard), &fSkidApproachHeight},
                {"Skid low extra height", 0x00412CCEu, 0x00412CD0u, kSkidLowExtraGuard, sizeof(kSkidLowExtraGuard), &fSkidLowExtraHeight},
                {"Skid strike start distance", 0x00412D24u, 0x00412D26u, kStrikeDistGuard, sizeof(kStrikeDistGuard), &fSkidStrikeStartDistance},
                {"Skid strike right-dot requirement", 0x00412D4Fu, 0x00412D51u, kStrikeDotGuard, sizeof(kStrikeDotGuard), &fSkidStrikeRightDotRequirement},
                {"Skid strike velocity lead X", 0x00412DADu, 0x00412DAFu, kStrikeVelGuard, sizeof(kStrikeVelGuard), &fSkidStrikeVelocityLead},
                {"Skid strike velocity lead Y", 0x00412DB7u, 0x00412DB9u, kStrikeVelGuard, sizeof(kStrikeVelGuard), &fSkidStrikeVelocityLead},
                {"Skid strike velocity lead Z", 0x00412DC1u, 0x00412DC3u, kStrikeVelGuard, sizeof(kStrikeVelGuard), &fSkidStrikeVelocityLead},
                {"Skid strike backscale X", 0x00412DD1u, 0x00412DD3u, kStrikeBackGuard, sizeof(kStrikeBackGuard), &fSkidStrikeBackScale},
                {"Skid strike backscale Y", 0x00412DE3u, 0x00412DE5u, kStrikeBackGuard, sizeof(kStrikeBackGuard), &fSkidStrikeBackScale},
                {"Skid strike backscale Z", 0x00412DF5u, 0x00412DF7u, kStrikeBackGuard, sizeof(kStrikeBackGuard), &fSkidStrikeBackScale},
                {"Skid abort ahead distance sq", 0x00412ED9u, 0x00412EDBu, kAbortAheadGuard, sizeof(kAbortAheadGuard), &fSkidAbortDistanceAheadSq},
                {"Skid abort behind distance sq", 0x00412EFCu, 0x00412EFEu, kAbortBehindGuard, sizeof(kAbortBehindGuard), &fSkidAbortDistanceBehindSq},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }
    }

    static void ApplyAIVehicleVelocityPatches() {
        const FloatOperandPatch patches[] = {
            {"Destination velocity multiplier X", 0x006A204Fu, 0x006A2051u, kDestVelMultGuard, sizeof(kDestVelMultGuard), &fDestinationVelocityMultiplier},
            {"Destination velocity multiplier Y", 0x006A205Bu, 0x006A205Du, kDestVelMultGuard, sizeof(kDestVelMultGuard), &fDestinationVelocityMultiplier},
            {"Destination velocity multiplier Z", 0x006A2067u, 0x006A2069u, kDestVelMultGuard, sizeof(kDestVelMultGuard), &fDestinationVelocityMultiplier},
            {"Destination velocity blend X", 0x006A2075u, 0x006A2077u, kDestVelBlendGuard, sizeof(kDestVelBlendGuard), &fDestinationVelocityBlend},
            {"Destination velocity blend Y", 0x006A2083u, 0x006A2085u, kDestVelBlendGuard, sizeof(kDestVelBlendGuard), &fDestinationVelocityBlend},
            {"Destination velocity blend Z", 0x006A2092u, 0x006A2094u, kDestVelBlendGuard, sizeof(kDestVelBlendGuard), &fDestinationVelocityBlend},
        };
        for (const auto& p : patches) PatchFloatOperand(p);
    }

    static void ApplyAIVehicleOnDrivingPatches() {
        // These are the PC globals/instruction sites that line up with the uploaded AIVehicleHelicopter::OnDriving layer.
        // They affect how hard the SimpleChopper layer brakes, caps speed, turns, and smooths desired vectors.
        if (gCfg.EnableAIVehicleOnDrivingDataPatches) {
            WriteFloatDataGuarded("Max_Chopper_Accel", 0x008F8DCCu, 80.0f, gCfg.MaxChopperAccel);
            WriteFloatDataGuarded("Min_Chopper_Accel", 0x008F8DD0u, 30.0f, gCfg.MinChopperAccel);
            WriteFloatDataGuarded("Chopper speed-limit ratio", 0x008F8DD4u, 2.0f, gCfg.ChopperSpeedLimitRatio);

            const FloatOperandPatch p{ "OnDriving decel speed scale", 0x006A2579u, 0x006A257Bu,
                                      kOnDriveDecelSpeedScaleGuard, sizeof(kOnDriveDecelSpeedScaleGuard), &fOnDrivingDecelSpeedScale };
            PatchFloatOperand(p);
        }

        if (gCfg.ForceOnDrivingMaxSpeedCap) {
            PatchBytes("Force OnDriving high speed/decel cap",
                0x006A25A7u,
                kForceOnDriveMaxSpeedCapGuard,
                sizeof(kForceOnDriveMaxSpeedCapGuard),
                kNop2,
                sizeof(kNop2));
        }

        if (gCfg.EnableAIVehicleOnDrivingTurnPatches) {
            const FloatOperandPatch patches[] = {
                {"OnDriving turn response scale", 0x006A28A8u, 0x006A28AAu, kOnDriveTurnScaleGuard, sizeof(kOnDriveTurnScaleGuard), &fOnDrivingTurnResponseScale},
                {"OnDriving turn clamp positive cmp", 0x006A28BAu, 0x006A28BCu, kOnDriveTurnClampPosCmpGuard, sizeof(kOnDriveTurnClampPosCmpGuard), &fOnDrivingTurnClampPositive},
                {"OnDriving turn clamp positive load", 0x006A2994u, 0x006A2996u, kOnDriveTurnClampPosLoadGuard, sizeof(kOnDriveTurnClampPosLoadGuard), &fOnDrivingTurnClampPositive},
                {"OnDriving turn clamp negative load A", 0x006A28CFu, 0x006A28D1u, kOnDriveTurnClampNegLoadGuard, sizeof(kOnDriveTurnClampNegLoadGuard), &fOnDrivingTurnClampNegative},
                {"OnDriving turn clamp negative load B", 0x006A28E2u, 0x006A28E4u, kOnDriveTurnClampNegLoadGuard, sizeof(kOnDriveTurnClampNegLoadGuard), &fOnDrivingTurnClampNegative},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }

        if (gCfg.EnableAIVehicleOnDrivingSmoothingPatches) {
            const FloatOperandPatch patches[] = {
                {"OnDriving smoothing old weight X", 0x006A28EFu, 0x006A28F1u, kOnDriveSmoothingOldWeightGuard, sizeof(kOnDriveSmoothingOldWeightGuard), &fOnDrivingSmoothingOldWeight},
                {"OnDriving smoothing old weight Y", 0x006A28FCu, 0x006A28FEu, kOnDriveSmoothingOldWeightGuard, sizeof(kOnDriveSmoothingOldWeightGuard), &fOnDrivingSmoothingOldWeight},
                {"OnDriving smoothing old weight Z", 0x006A2908u, 0x006A290Au, kOnDriveSmoothingOldWeightGuard, sizeof(kOnDriveSmoothingOldWeightGuard), &fOnDrivingSmoothingOldWeight},
                {"OnDriving smoothing final scale X", 0x006A291Eu, 0x006A2920u, kOnDriveSmoothingFinalScaleGuard, sizeof(kOnDriveSmoothingFinalScaleGuard), &fOnDrivingSmoothingFinalScale},
                {"OnDriving smoothing final scale Y", 0x006A292Bu, 0x006A292Du, kOnDriveSmoothingFinalScaleGuard, sizeof(kOnDriveSmoothingFinalScaleGuard), &fOnDrivingSmoothingFinalScale},
                {"OnDriving smoothing final scale Z", 0x006A293Cu, 0x006A293Eu, kOnDriveSmoothingFinalScaleGuard, sizeof(kOnDriveSmoothingFinalScaleGuard), &fOnDrivingSmoothingFinalScale},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }
    }

    static void ApplyHeliSheetPatches() {
        // EA behavior map: Update() resets bIgnoreHeliSheet false each tick, then sets true for skid approach/strike
        // unless NeverIgnoreHeliSheet is true. These two globals are the PC addresses matched from the update function.
        static constexpr uintptr_t kNeverIgnoreHeliSheetVA = 0x008EB1F4u;

        WriteBoolData("NeverIgnoreHeliSheet", kNeverIgnoreHeliSheetVA, gCfg.RespectHeliSheetDuringSkid ? 1 : 0);

        if (gCfg.ForceIgnoreHeliSheetInAllPursuitModes) {
            PatchImm8("Force bIgnoreHeliSheet=true at pursuit switch",
                0x004278A5u,
                kHeliSheetBaseWriteGuard,
                sizeof(kHeliSheetBaseWriteGuard),
                0x004278ABu,
                1);
        }
    }


    static void ApplyExitFuelAndActionPatches() {
        // AIVehicleHelicopter::UpdateFuel: stock decrements fuel and, once <= 0, forces AIGoalHeliExit.
        // 0x0042352E is the conditional skip-over-exit branch. Turning it into an unconditional jump
        // keeps the heli in the active pursuit goal instead of bailing out when the timer reaches zero.
        if (gCfg.DisableFuelBasedHeliExit) {
            static const uint8_t kJumpAlways[] = { 0xEB, 0x4C };
            PatchBytes("Disable fuel-based AIGoalHeliExit", 0x0042352Eu, kFuelExitBranchGuard, sizeof(kFuelExitBranchGuard), kJumpAlways, sizeof(kJumpAlways));
        }

        if (!gCfg.EnableHeliExitActionPatches) return;

        if (gCfg.PatchExitFlySpeed) {
            PatchImm32("AIActionHeliExit fly speed", 0x00427BFEu, kExitFlySpeedPushGuard, sizeof(kExitFlySpeedPushGuard), 0x00427BFFu, FloatBits(fExitFlySpeed));
        }

        if (gCfg.PatchExitSeekUpThreshold) {
            const FloatOperandPatch p{ "AIActionHeliExit seek-up transition height", 0x00427B84u, 0x00427B86u,
                                      kExitSeekUpThresholdGuard, sizeof(kExitSeekUpThresholdGuard), &fExitSeekUpThreshold };
            PatchFloatOperand(p);
        }

        if (gCfg.PatchExitSeekAheadDistance) {
            const FloatOperandPatch patches[] = {
                {"AIActionHeliExit seek-ahead X", 0x00427B95u, 0x00427B97u, kExitSeekAheadGuard, sizeof(kExitSeekAheadGuard), &fExitSeekAheadDistance},
                {"AIActionHeliExit seek-ahead Y", 0x00427BA6u, 0x00427BA8u, kExitSeekAheadGuard, sizeof(kExitSeekAheadGuard), &fExitSeekAheadDistance},
                {"AIActionHeliExit seek-ahead Z", 0x00427BB7u, 0x00427BB9u, kExitSeekAheadGuard, sizeof(kExitSeekAheadGuard), &fExitSeekAheadDistance},
                {"AIActionHeliExit seek-car height", 0x00427BC4u, 0x00427BC6u, kExitSeekHeightGuard, sizeof(kExitSeekHeightGuard), &fExitFinishHeight},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }

        if (gCfg.PatchExitSeekCarReachDistance) {
            const FloatOperandPatch p{ "AIActionHeliExit seek-car reach distance sq", 0x00427AA0u, 0x00427AA2u,
                                      kExitReachDistanceSqGuard, sizeof(kExitReachDistanceSqGuard), &fExitSeekCarReachDistanceSq };
            PatchFloatOperand(p);
        }

        if (gCfg.PatchExitFlyoutBackScale) {
            const FloatOperandPatch patches[] = {
                {"AIActionHeliExit right negative scale", 0x00427AF8u, 0x00427AFAu, kExitRightNegativeScaleGuard, sizeof(kExitRightNegativeScaleGuard), &fExitRightNegativeScale},
                {"AIActionHeliExit flyout backscale X", 0x00427B34u, 0x00427B36u, kExitFlyoutBackScaleGuard, sizeof(kExitFlyoutBackScaleGuard), &fExitFlyoutBackScale},
                {"AIActionHeliExit flyout backscale Y", 0x00427B45u, 0x00427B47u, kExitFlyoutBackScaleGuard, sizeof(kExitFlyoutBackScaleGuard), &fExitFlyoutBackScale},
                {"AIActionHeliExit flyout backscale Z", 0x00427B56u, 0x00427B58u, kExitFlyoutBackScaleGuard, sizeof(kExitFlyoutBackScaleGuard), &fExitFlyoutBackScale},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }

        if (gCfg.PatchExitFlyoutExtraHeight) {
            const FloatOperandPatch p{ "AIActionHeliExit flyout extra height", 0x00427B74u, 0x00427B76u,
                                      kExitFlyoutExtraHeightGuard, sizeof(kExitFlyoutExtraHeightGuard), &fExitFlyoutExtraHeight };
            PatchFloatOperand(p);
        }

        if (gCfg.PatchExitFinishRules) {
            const FloatOperandPatch patches[] = {
                {"AIActionHeliExit finish height", 0x0042794Eu, 0x00427950u, kExitHeightCompareGuard, sizeof(kExitHeightCompareGuard), &fExitFinishHeight},
                {"AIActionHeliExit finish distance sq", 0x004279C8u, 0x004279CAu, kExitFinishDistanceSqGuard, sizeof(kExitFinishDistanceSqGuard), &fExitFinishDistanceSq},
            };
            for (const auto& p : patches) PatchFloatOperand(p);
        }

        if (gCfg.PatchExitDoDrivingMode) {
            PatchImm8("AIActionHeliExit DoDriving mode", 0x00427C28u, kExitDoDrivingPushGuard, sizeof(kExitDoDrivingPushGuard), 0x00427C29u, gCfg.ExitDoDrivingMode);
        }
    }

    static void ApplyLegacyPatch() {
        const FloatOperandPatch p{ "Legacy render/distance threshold", 0x0069CF71u, 0x0069CF73u, kLegacyRenderGuard, sizeof(kLegacyRenderGuard), &fLegacyRenderDistanceThreshold };
        PatchFloatOperand(p);
    }

    static void ApplyHeliRenderConnPatches() {
        // HeliRenderConn::OnFetch in the PC EXE constructs RenderConn::Pkt_Heli_Service.
        // Source behavior: wasdrawn = mLastVisibleFrame >= mLastRenderFrame && mLastRenderFrame != 0;
        // then the packet sends (wasdrawn, mDistanceToView) through Service(). This only affects
        // visibility/service feedback, not steering. Use it as an experimental "keep the heli serviced" layer.
        if (gCfg.ForceHeliRenderServiceInView) {
            static const uint8_t kForceInViewPatch[] = {
                0xB0,0x01,             // mov al, 1
                0xEB,0x14,             // jump to the distance load at 0x00739513
                0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,
                0x90,0x90,0x90,0x90,0x90,0x90,0x90,0x90,
                0x90,0x90,0x90,0x90
            };
            PatchBytes("HeliRenderConn force Pkt_Heli_Service in-view", 0x007394FBu,
                kHeliRenderForceServiceInViewGuard, sizeof(kHeliRenderForceServiceInViewGuard),
                kForceInViewPatch, sizeof(kForceInViewPatch));
        }

        if (gCfg.ForceHeliRenderServiceDistance) {
            const FloatOperandPatch p{ "HeliRenderConn service distance report", 0x00739513u, 0x00739515u,
                                      kHeliRenderServiceDistanceGuard, sizeof(kHeliRenderServiceDistanceGuard),
                                      &fHeliRenderServiceDistance };
            PatchFloatOperand(p);
        }

        if (gCfg.PreventHeliRenderHideOnServiceFail) {
            static const uint8_t kNop4[] = { 0x90,0x90,0x90,0x90 };
            PatchBytes("HeliRenderConn prevent Hide() on Service() fail", 0x00739568u,
                kHeliRenderServiceFailHideGuard, sizeof(kHeliRenderServiceFailHideGuard),
                kNop4, sizeof(kNop4));
        }

        if (gCfg.PatchHeliRenderResetDistance) {
            PatchImm32("HeliRenderConn OnRender reset distance", 0x007511FCu,
                kHeliRenderResetDistanceGuard, sizeof(kHeliRenderResetDistanceGuard),
                0x00751202u, FloatBits(fHeliRenderResetDistance));
        }
    }

    static void ApplyDispatchSpawnPatches() {
        // 0x0042BA50 is the weighted pursuit-support selection function exposed by the copheli xrefs.
        // It can return "copheli" directly through a request flag, or choose from a weighted list.
        // Stock code zeroes the copheli weight when the internal heli-allowed gate is false.
        if (gCfg.AllowCopheliWeightedSelection) {
            static const uint8_t kJmpShort0E[] = { 0xEB,0x0E };
            PatchBytes("Allow copheli in weighted selector",
                0x0042BC54u,
                kCopheliWeightedSelectionGateGuard,
                sizeof(kCopheliWeightedSelectionGateGuard),
                kJmpShort0E,
                sizeof(kJmpShort0E));
        }

        // Still leaves normal chance/timer logic intact, but ignores the global that blocks a selector-triggered heli
        // request when a heli is already involved. This is riskier than AllowCopheliWeightedSelection.
        if (gCfg.IgnoreExistingHeliForSelectorTrigger) {
            PatchBytes("Ignore existing-heli gate in selector trigger",
                0x0042BB04u,
                kSelectorExistingHeliGateGuard,
                sizeof(kSelectorExistingHeliGateGuard),
                kNop2,
                sizeof(kNop2));
        }

        // 0x004269A0 is the special copheli creation helper called from the spawn manager when a pending item
        // resolves to "copheli". Stock code immediately fails if DAT_0090D61C says a heli is involved.
        if (gCfg.IgnoreExistingHeliForSpecialSpawn) {
            PatchBytes("Ignore existing-heli gate in special copheli spawn",
                0x004269CAu,
                kSpecialSpawnExistingHeliGateGuard,
                sizeof(kSpecialSpawnExistingHeliGateGuard),
                kNop6,
                sizeof(kNop6));
        }

        // Emergency test only: makes the selector's "if request flag then return copheli" path fall through every time.
        // This can turn many support selections into copheli and should not be used as a normal setting.
        if (gCfg.ForceWeightedSelectorAlwaysReturnCopheli) {
            PatchBytes("Force weighted selector to return copheli",
                0x0042BB5Au,
                kForceSelectorReturnCopheliGuard,
                sizeof(kForceSelectorReturnCopheliGuard),
                kNop2,
                sizeof(kNop2));
        }

        // Risky cap experiment: the spawn manager normally skips the immediate string-create path once the active
        // pursuit count reaches the configured max. This NOPs that cap branch for all immediate requests, not just heli.
        if (gCfg.BypassSpawnCapForImmediateRequests) {
            PatchBytes("Bypass spawn cap for immediate request path",
                0x0043EB90u,
                kSpawnCapImmediateRequestGuard,
                sizeof(kSpawnCapImmediateRequestGuard),
                kNop2,
                sizeof(kNop2));
        }
    }

    static void ApplyGhidraExtraPatches() {
        // From AIGoalHeliRoadBlock.c: 0x00423430 chooses between AIGoalHeliRoadBlock and
        // AIGoalStaticRoadBlock after comparing the current model/type against DAT_0092C4F4.
        // NOPing the JNE at 0x00423482 forces the heli-roadblock goal-name path. This is experimental
        // and OFF by default because it can make non-heli roadblock dispatch choose the heli goal too.
        if (gCfg.ForceAIGoalHeliRoadBlockSelection) {
            PatchBytes("Force roadblock selector to AIGoalHeliRoadBlock",
                0x00423482u,
                kRoadblockGoalBranchGuard,
                sizeof(kRoadblockGoalBranchGuard),
                kNop2,
                sizeof(kNop2));
        }

        // From HeliWash.c: constructor area pushes 0x3F800000 as a default Physics scalar for HeliWash.
        // This does not make the heli smarter, but it is a safe guarded knob for testing rotor-wash strength.
        if (gCfg.EnableHeliWashPatches) {
            PatchImm32("HeliWash default Physics scalar",
                0x006BA198u,
                kHeliWashPhysicsStrengthPushGuard,
                sizeof(kHeliWashPhysicsStrengthPushGuard),
                0x006BA199u,
                FloatBits(fHeliWashPhysicsStrength));
        }
    }

    static DWORD WINAPI WorkerThread(void*) {
        Sleep(1000);
        LoadConfig();
        OpenLogFile();
        Log("Worker started.");
        LogConfigSummary();

        if (!ValidateSpeedExe()) {
            Log("Validation failed; not patching.");
            CloseLogFile();
            return 0;
        }

        if (!gCfg.EnableSkidLogicPatches) {
            Log("Skid logic patches disabled.");
        }
        else {
            ApplySkidLogicPatches();
        }

        if (gCfg.EnableAIVehicleVelocityPatches) {
            ApplyAIVehicleVelocityPatches();
        }

        ApplyAIVehicleOnDrivingPatches();

        ApplyExitFuelAndActionPatches();

        if (gCfg.EnableHeliSheetPatches) {
            ApplyHeliSheetPatches();
        }

        ApplyVisionPatches();
        ApplyChopperSpeedPatches();

        if (gCfg.EnableLegacyRenderDistancePatch) {
            ApplyLegacyPatch();
        }

        if (gCfg.EnableHeliRenderConnPatches) {
            ApplyHeliRenderConnPatches();
        }

        if (gCfg.EnableGhidraExtraPatches) {
            ApplyGhidraExtraPatches();
        }

        if (gCfg.EnableDispatchSpawnPatches) {
            ApplyDispatchSpawnPatches();
        }

        ApplyHeatObserverPatches();

        if (gCfg.EnableHeatBasedProfiles && gCurrentHeatLevel >= 1 && gCurrentHeatLevel <= 10) {
            ApplyHeatProfile(gCurrentHeatLevel, "after patch pass");
        }


        Log("Patch pass finished. Applied=%d Skipped=%d Failed=%d",
            gPatchAppliedCount, gPatchSkippedCount, gPatchFailedCount);
        if (gPatchFailedCount == 0) {
            Log("Release check: no write failures were reported. Review skipped guards before publishing if you changed EXE builds.");
        }
        else {
            Log("Release check: NOT ready. One or more writes failed; check failed lines above.");
        }
        if (gCfg.EnableChopperSpeed) {
            Log("Chopper::Speed is enabled; leaving log file open for live speed diagnostics.");
        }
        else {
            CloseLogFile();
        }
        return 0;
    }

} // namespace HelicopterOptions

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HelicopterOptions::gModule = hModule;
        DisableThreadLibraryCalls(hModule);
        HANDLE thread = CreateThread(nullptr, 0, HelicopterOptions::WorkerThread, nullptr, 0, nullptr);
        if (thread) CloseHandle(thread);
    }
    return TRUE;
}
