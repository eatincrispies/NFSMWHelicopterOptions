// Movement.cpp - SimpleChopper motion layer (FUN_006A2030 / FUN_006A2430):
// acceleration budget, turn response/clamp, and the two normalized filters.
// Filter pairs are gain-checked in Validation::Sanitize before this runs.
#include "Systems.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/PatchManager.h"
#include "../Core/Log.h"

namespace Systems {

    void ApplyAcceleration() {
        if (!gCfg.EnableAccelPatches && !gCfg.ForceMaxAccelAuthority) {
            Log::Verbose("[ChopperAcceleration] disabled.");
            return;
        }

        if (gCfg.EnableAccelPatches) {
            Patch::Begin("ChopperAcceleration");
            Patch::AddDataFloat("Max_Chopper_Accel (vanilla 80)", Addr::kMaxChopperAccel,
                                Addr::Vanilla::MaxChopperAccel, gCfg.AccelBudgetMax);
            Patch::AddDataFloat("Min_Chopper_Accel (vanilla 30)", Addr::kMinChopperAccel,
                                Addr::Vanilla::MinChopperAccel, gCfg.AccelBudgetMin);
            Patch::AddDataFloat("Chopper_Ratio (vanilla 2)", Addr::kChopperRatio,
                                Addr::Vanilla::ChopperRatio, gCfg.VelocityErrorToAccelRatio);
            Patch::AddFloatOperand("Accel budget speed scale (vanilla 0.6)",
                                   Addr::Site::AccelBudgetSpeedScale,
                                   Addr::Guard::AccelSpeedScale, &gLive.AccelBudgetSpeedScale);
            Patch::Commit();
        }

        if (gCfg.ForceMaxAccelAuthority) {
            static const uint8_t kNop2[2] = { 0x90, 0x90 };
            Patch::Begin("ChopperAcceleration::ForceMaxAccelAuthority (Experimental)");
            Patch::AddBytes("Force full accel budget branch", Addr::Site::ForceMaxAccelBranch,
                            Addr::Guard::ForceMaxAccel, 2, kNop2, 2);
            Patch::Commit();
        }
    }

    void ApplySteering() {
        if (!gCfg.EnableSteeringPatches) { Log::Verbose("[ChopperSteering] disabled."); return; }
        Patch::Begin("ChopperSteering");
        Patch::AddFloatOperand("Turn response scale (vanilla -8)", Addr::Site::TurnResponseScale,
                               Addr::Guard::TurnResponse, &gLive.TurnResponseScale);
        Patch::AddFloatOperand("Turn clamp + compare (vanilla 1.3)", Addr::Site::TurnClampPosCmp,
                               Addr::Guard::TurnClampPosCmp, &gLive.TurnClampPos);
        Patch::AddFloatOperand("Turn clamp + load (vanilla 1.3)", Addr::Site::TurnClampPosLoad,
                               Addr::Guard::TurnClampPosLoad, &gLive.TurnClampPos);
        Patch::AddFloatOperand("Turn clamp - load A (vanilla -1.3)", Addr::Site::TurnClampNegLoadA,
                               Addr::Guard::TurnClampNegLoad, &gLive.TurnClampNeg);
        Patch::AddFloatOperand("Turn clamp - load B (vanilla -1.3)", Addr::Site::TurnClampNegLoadB,
                               Addr::Guard::TurnClampNegLoad, &gLive.TurnClampNeg);
        Patch::Commit();
    }

    void ApplySmoothing() {
        if (!gCfg.EnableSmoothingPatches) { Log::Verbose("[ChopperSmoothing] disabled."); return; }

        if (gCfg.PatchOutputSmoothing) {
            Patch::Begin("ChopperSmoothing::Output");
            Patch::AddFloatOperand("Output smoothing old weight X (vanilla 7)", Addr::Site::SmoothOldWeightX,
                                   Addr::Guard::SmoothOldWeight, &gLive.SmoothingOldWeight);
            Patch::AddFloatOperand("Output smoothing old weight Y", Addr::Site::SmoothOldWeightY,
                                   Addr::Guard::SmoothOldWeight, &gLive.SmoothingOldWeight);
            Patch::AddFloatOperand("Output smoothing old weight Z", Addr::Site::SmoothOldWeightZ,
                                   Addr::Guard::SmoothOldWeight, &gLive.SmoothingOldWeight);
            Patch::AddFloatOperand("Output smoothing final scale X (vanilla 0.125)", Addr::Site::SmoothFinalScaleX,
                                   Addr::Guard::SmoothFinalScale, &gLive.SmoothingFinalScale);
            Patch::AddFloatOperand("Output smoothing final scale Y", Addr::Site::SmoothFinalScaleY,
                                   Addr::Guard::SmoothFinalScale, &gLive.SmoothingFinalScale);
            Patch::AddFloatOperand("Output smoothing final scale Z", Addr::Site::SmoothFinalScaleZ,
                                   Addr::Guard::SmoothFinalScale, &gLive.SmoothingFinalScale);
            Patch::Commit();
        }

        if (gCfg.PatchDestVelFilter) {
            Patch::Begin("ChopperSmoothing::DestVelFilter");
            Patch::AddFloatOperand("Dest-vel filter old weight X (vanilla 4)", Addr::Site::DestVelWeightX,
                                   Addr::Guard::DestVelWeight, &gLive.DestVelFilterOldWeight);
            Patch::AddFloatOperand("Dest-vel filter old weight Y", Addr::Site::DestVelWeightY,
                                   Addr::Guard::DestVelWeight, &gLive.DestVelFilterOldWeight);
            Patch::AddFloatOperand("Dest-vel filter old weight Z", Addr::Site::DestVelWeightZ,
                                   Addr::Guard::DestVelWeight, &gLive.DestVelFilterOldWeight);
            Patch::AddFloatOperand("Dest-vel filter final scale X (vanilla 0.2)", Addr::Site::DestVelScaleX,
                                   Addr::Guard::DestVelScale, &gLive.DestVelFilterFinalScale);
            Patch::AddFloatOperand("Dest-vel filter final scale Y", Addr::Site::DestVelScaleY,
                                   Addr::Guard::DestVelScale, &gLive.DestVelFilterFinalScale);
            Patch::AddFloatOperand("Dest-vel filter final scale Z", Addr::Site::DestVelScaleZ,
                                   Addr::Guard::DestVelScale, &gLive.DestVelFilterFinalScale);
            Patch::Commit();
        }
    }


    // ------------------------------------------------------------------ high FPS
    // The chopper motion update derives its own velocity as
    //   velocity = (position - lastPosition) / dt
    // and guards that division with  if (dt > 0.005). At 200 FPS and above the
    // guard fails every frame, the derivation is skipped, the destination-
    // velocity filter is never fed, and the X/Z channels of the motion command
    // decay to zero - the helicopter stops swaying and holds station. Lowering
    // the threshold restores the derivation. The 0.005 constant itself is
    // shared with five unrelated call sites, so only THIS operand is
    // redirected, at a mod-owned float.
    void ApplyHighFpsFix() {
        if (!gCfg.EnableFrameRateFix) {
            Log::Verbose("[FrameRate] high-FPS motion fix disabled.");
            return;
        }
        Patch::Begin("FrameRate::HighFpsMotionFix");
        Patch::AddFloatOperand("Chopper velocity derive threshold (vanilla 0.005 = 200 FPS)",
                               Addr::Site::ChopperVelDtGate,
                               Addr::Guard::ChopperVelDtGate, &gLive.ChopperVelDtGate);
        if (Patch::Commit())
            Log::Info("High frame rates: the helicopter keeps deriving its own motion "
                      "above 200 FPS (threshold lowered to %.4f ms).",
                      kFrameMinPhysicsDelta * 1000.0f);
    }

} // namespace Systems