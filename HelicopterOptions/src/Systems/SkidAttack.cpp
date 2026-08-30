// SkidAttack.cpp - skid entry gates (StraightLinePursuit) and the
// approach/strike/abort tuning (SkidHitPursuit). All sites exe-verified;
// containing functions confirmed in the Ghidra dump against EA source.
#include "Systems.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/PatchManager.h"
#include "../Core/Log.h"

namespace Systems {

    void ApplySkidEntry() {
        if (!gCfg.EnableSkidEntryPatches && !gCfg.ForceSkidHitAttribute) {
            Log::Verbose("[SkidEntry] disabled.");
            return;
        }

        if (gCfg.EnableSkidEntryPatches) {
            Patch::Begin("SkidEntry");
            Patch::AddFloatOperand("Skid cooldown gate", Addr::Site::SkidCooldownGate,
                                   Addr::Guard::SkidCooldown, &gLive.SkidCooldownThreshold);
            Patch::AddFloatOperand("Skid cooldown height-mode threshold", Addr::Site::SkidCooldownHeight,
                                   Addr::Guard::SkidCooldown, &gLive.SkidCooldownThreshold);
            Patch::AddFloatOperand("Skid entry max distance (vanilla 35)", Addr::Site::EntryMaxDistance,
                                   Addr::Guard::EntryMaxDist, &gLive.SkidEntryMaxDistance);
            Patch::AddFloatOperand("Skid entry min distance (vanilla 5)", Addr::Site::EntryMinDistance,
                                   Addr::Guard::EntryMinDist, &gLive.SkidEntryMinDistance);
            Patch::AddFloatOperand("Skid entry alignment dot (vanilla 0.707)", Addr::Site::EntryAlignmentDot,
                                   Addr::Guard::EntryDot, &gLive.SkidEntryAlignmentDot);
            Patch::AddFloatOperand("Skid entry max height delta (vanilla 13)", Addr::Site::EntryMaxHeightDelta,
                                   Addr::Guard::EntryYDelta, &gLive.SkidEntryMaxHeightDelta);
            Patch::Commit();
        }

        if (gCfg.ForceSkidHitAttribute) {
            static const uint8_t kNop2[2] = { 0x90, 0x90 };
            Patch::Begin("SkidEntry::ForceSkidHitAttribute (Experimental)");
            Patch::AddBytes("Force SkidHitEnabled branch", Addr::Site::ForceSkidAttrBranch,
                            Addr::Guard::ForceSkidAttr, 2, kNop2, 2);
            Patch::Commit();
        }
    }

    void ApplyLead() {
        if (!gCfg.EnableLeadPatches) { Log::Verbose("[ChopperLead] disabled."); return; }
        Patch::Begin("ChopperLead");
        Patch::AddFloatOperand("Lead speed scale (vanilla 0.4)", Addr::Site::LeadSpeedScale,
                               Addr::Guard::LeadSpeedScale, &gLive.LeadSpeedScale);
        Patch::AddFloatOperand("Lead base (vanilla 30)", Addr::Site::LeadBase,
                               Addr::Guard::LeadBase, &gLive.LeadBase);
        Patch::AddFloatOperand("Lead max (vanilla 45)", Addr::Site::LeadMax,
                               Addr::Guard::LeadMax, &gLive.LeadMax);
        Patch::AddFloatOperand("Lead skid multiplier (vanilla 0.75)", Addr::Site::LeadSkidMultiplier,
                               Addr::Guard::LeadSkidMult, &gLive.LeadSkidMultiplier);
        Patch::Commit();
    }

    void ApplyAltitude() {
        if (!gCfg.EnableAltitudePatches) { Log::Verbose("[ChopperAltitude] disabled."); return; }
        Patch::Begin("ChopperAltitude");
        Patch::AddFloatOperand("Chase height during skid window (vanilla 2)", Addr::Site::ChaseHeightSkid,
                               Addr::Guard::HeightSkid, &gLive.ChaseHeightSkid);
        Patch::AddFloatOperand("Chase height close (vanilla 6)", Addr::Site::ChaseHeightClose,
                               Addr::Guard::HeightClose, &gLive.ChaseHeightClose);
        Patch::AddFloatOperand("Chase height far/high (vanilla 12)", Addr::Site::ChaseHeightHigh,
                               Addr::Guard::HeightHigh, &gLive.ChaseHeightHigh);
        Patch::Commit();
    }

    void ApplySkidStrike() {
        if (!gCfg.EnableSkidStrikePatches) { Log::Verbose("[SkidStrike] disabled."); return; }
        Patch::Begin("SkidStrike");
        Patch::AddFloatOperand("Side offset + (vanilla 6)", Addr::Site::SideOffsetPos,
                               Addr::Guard::SidePos, &gLive.SideOffsetPos);
        Patch::AddFloatOperand("Side offset - (vanilla -6)", Addr::Site::SideOffsetNeg,
                               Addr::Guard::SideNeg, &gLive.SideOffsetNeg);
        Patch::AddFloatOperand("Approach velocity lead X (vanilla 0.23)", Addr::Site::ApproachVelX,
                               Addr::Guard::ApproachVel, &gLive.ApproachVelocityLead);
        Patch::AddFloatOperand("Approach velocity lead Y", Addr::Site::ApproachVelY,
                               Addr::Guard::ApproachVel, &gLive.ApproachVelocityLead);
        Patch::AddFloatOperand("Approach velocity lead Z", Addr::Site::ApproachVelZ,
                               Addr::Guard::ApproachVel, &gLive.ApproachVelocityLead);
        Patch::AddFloatOperand("Approach height (vanilla 1.8)", Addr::Site::ApproachHeight,
                               Addr::Guard::ApproachHeight, &gLive.ApproachHeight);
        Patch::AddFloatOperand("Low extra height (vanilla 3)", Addr::Site::LowExtraHeight,
                               Addr::Guard::LowExtra, &gLive.LowExtraHeight);
        Patch::AddFloatOperand("Strike start distance (vanilla 4)", Addr::Site::StrikeStartDistance,
                               Addr::Guard::StrikeDist, &gLive.StrikeStartDistance);
        Patch::AddFloatOperand("Strike lateral trigger distance METERS (vanilla 1.9)",
                               Addr::Site::StrikeLateralTrigger,
                               Addr::Guard::StrikeLateral, &gLive.StrikeLateralTrigger);
        Patch::AddFloatOperand("Strike velocity lead X (vanilla 0.092)", Addr::Site::StrikeVelX,
                               Addr::Guard::StrikeVel, &gLive.StrikeVelocityLead);
        Patch::AddFloatOperand("Strike velocity lead Y", Addr::Site::StrikeVelY,
                               Addr::Guard::StrikeVel, &gLive.StrikeVelocityLead);
        Patch::AddFloatOperand("Strike velocity lead Z", Addr::Site::StrikeVelZ,
                               Addr::Guard::StrikeVel, &gLive.StrikeVelocityLead);
        Patch::AddFloatOperand("Strike back scale X (vanilla -0.5)", Addr::Site::StrikeBackX,
                               Addr::Guard::StrikeBack, &gLive.StrikeBackScale);
        Patch::AddFloatOperand("Strike back scale Y", Addr::Site::StrikeBackY,
                               Addr::Guard::StrikeBack, &gLive.StrikeBackScale);
        Patch::AddFloatOperand("Strike back scale Z", Addr::Site::StrikeBackZ,
                               Addr::Guard::StrikeBack, &gLive.StrikeBackScale);
        Patch::AddFloatOperand("Abort ahead distance sq (vanilla 1600)", Addr::Site::AbortAheadSq,
                               Addr::Guard::AbortAhead, &gLive.AbortAheadSq);
        Patch::AddFloatOperand("Abort behind distance sq (vanilla 144)", Addr::Site::AbortBehindSq,
                               Addr::Guard::AbortBehind, &gLive.AbortBehindSq);
        Patch::Commit();
    }

} // namespace Systems
