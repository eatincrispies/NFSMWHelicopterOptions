// ExitBehavior.cpp - fuel-based exit control and AIActionHeliExit tuning.
// Vanilla values (exe-read): fly speed 100, seek-up 15, seek-ahead 85,
// reach 25, right -9, back -200, extra +5, Exit_Height 25, finish 22500.
#include "Systems.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/PatchManager.h"
#include "../Core/Log.h"
#include <cstring>

namespace Systems {

    namespace {
        uint32_t FloatBits(float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; }
    }

    void ApplyExitBehavior() {
        if (gCfg.DisableFuelBasedExit) {
            static const uint8_t kJmpAlways[2] = { 0xEB, 0x4C };
            Patch::Begin("ExitBehavior::DisableFuelBasedExit");
            Patch::AddBytes("Disable fuel-forced AIGoalHeliExit", Addr::Site::FuelExitBranch,
                            Addr::Guard::FuelExitBranch, 2, kJmpAlways, 2);
            Patch::Commit();
        }

        if (!gCfg.EnableExitActionPatches) {
            Log::Verbose("[ExitBehavior] action patches disabled.");
            return;
        }

        if (gCfg.PatchExitFlySpeed) {
            Patch::Begin("ExitBehavior::FlySpeed");
            Patch::AddImm32("Exit fly speed (vanilla 100)", Addr::Site::ExitFlySpeedPush,
                            Addr::Guard::ExitFlySpeedPush, 5,
                            Addr::Site::ExitFlySpeedImm, FloatBits(gCfg.ExitFlySpeed));
            Patch::Commit();
        }

        if (gCfg.PatchExitSeekUpThreshold) {
            Patch::Begin("ExitBehavior::SeekUpThreshold");
            Patch::AddFloatOperand("Exit seek-up threshold (vanilla 15)", Addr::Site::ExitSeekUpThreshold,
                                   Addr::Guard::ExitSeekUp, &gLive.ExitSeekUpThreshold);
            Patch::Commit();
        }

        if (gCfg.PatchExitSeekAheadDistance) {
            Patch::Begin("ExitBehavior::SeekAheadDistance");
            Patch::AddFloatOperand("Exit seek-ahead X (vanilla 85)", Addr::Site::ExitSeekAheadX,
                                   Addr::Guard::ExitSeekAhead, &gLive.ExitSeekAheadDistance);
            Patch::AddFloatOperand("Exit seek-ahead Y", Addr::Site::ExitSeekAheadY,
                                   Addr::Guard::ExitSeekAhead, &gLive.ExitSeekAheadDistance);
            Patch::AddFloatOperand("Exit seek-ahead Z", Addr::Site::ExitSeekAheadZ,
                                   Addr::Guard::ExitSeekAhead, &gLive.ExitSeekAheadDistance);
            Patch::Commit();
        }

        if (gCfg.PatchExitSeekCarHeight) {
            Patch::Begin("ExitBehavior::SeekCarHeight");
            Patch::AddFloatOperand("Exit seek-car height (vanilla Exit_Height=25)",
                                   Addr::Site::ExitSeekCarHeight,
                                   Addr::Guard::ExitSeekCarHeight, &gLive.ExitTargetHeight);
            Patch::Commit();
        }

        if (gCfg.PatchExitReachDistance) {
            Patch::Begin("ExitBehavior::ReachDistance");
            Patch::AddFloatOperand("Exit seek-car reach distance sq (vanilla 25)",
                                   Addr::Site::ExitReachDistanceSq,
                                   Addr::Guard::ExitReachDistSq, &gLive.ExitSeekCarReachDistanceSq);
            Patch::Commit();
        }

        if (gCfg.PatchExitFlyout) {
            Patch::Begin("ExitBehavior::Flyout");
            Patch::AddFloatOperand("Exit flyout right scale (vanilla -9)", Addr::Site::ExitRightScale,
                                   Addr::Guard::ExitRightScale, &gLive.ExitRightScale);
            Patch::AddFloatOperand("Exit flyout back scale X (vanilla -200)", Addr::Site::ExitBackX,
                                   Addr::Guard::ExitBackScale, &gLive.ExitFlyoutBackScale);
            Patch::AddFloatOperand("Exit flyout back scale Y", Addr::Site::ExitBackY,
                                   Addr::Guard::ExitBackScale, &gLive.ExitFlyoutBackScale);
            Patch::AddFloatOperand("Exit flyout back scale Z", Addr::Site::ExitBackZ,
                                   Addr::Guard::ExitBackScale, &gLive.ExitFlyoutBackScale);
            Patch::AddFloatOperand("Exit flyout extra height (vanilla 5)", Addr::Site::ExitFlyoutExtraHeight,
                                   Addr::Guard::ExitExtraHeight, &gLive.ExitFlyoutExtraHeight);
            Patch::Commit();
        }

        if (gCfg.PatchExitFinishRules) {
            Patch::Begin("ExitBehavior::FinishRules");
            Patch::AddFloatOperand("Exit finish height (vanilla Exit_Height=25)",
                                   Addr::Site::ExitFinishHeight,
                                   Addr::Guard::ExitFinishHeight, &gLive.ExitTargetHeight);
            Patch::AddFloatOperand("Exit finish distance sq (vanilla 22500)",
                                   Addr::Site::ExitFinishDistanceSq,
                                   Addr::Guard::ExitFinishDistSq, &gLive.ExitFinishDistanceSq);
            Patch::Commit();
        }

        if (gCfg.PatchExitDoDrivingMode) {
            Patch::Begin("ExitBehavior::DoDrivingMode (Experimental)");
            Patch::AddImm8("Exit DoDriving mode (vanilla 7)", Addr::Site::ExitDoDrivingPush,
                           Addr::Guard::ExitDoDrivingPush, 2,
                           Addr::Site::ExitDoDrivingImm, gCfg.ExitDoDrivingMode);
            Patch::Commit();
        }
    }

} // namespace Systems
