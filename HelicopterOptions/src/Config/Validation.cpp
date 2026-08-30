// Validation.cpp - cross-field rules only. Per-key bounds live in the
// key table in Ini.cpp and are clamped (with warnings) at parse time.
#include <cmath>
#include "Validation.h"
#include "Config.h"
#include "../Core/Log.h"

namespace Validation {

    namespace {
        void EnforceFilterPair(const char* label, float* oldWeight, float* finalScale) {
            const float gain = (*oldWeight + 1.0f) * (*finalScale);
            if (gain > 1.0001f) {
                if (gCfg.AllowUnstableFilterGain) {
                    Log::Warn("%s gain %.3f > 1.0 with AllowUnstableFilterGain=1. "
                              "THIS AMPLIFIES MOTION EVERY TICK and is the verified spin "
                              "mechanism. You are on your own.", label, gain);
                } else {
                    const float fixedScale = 1.0f / (*oldWeight + 1.0f);
                    Log::Warn("%s gain %.3f > 1.0 (OldWeight=%.3f, FinalScale=%.3f). "
                              "FinalScale normalized to %.5f (gain 1.0).",
                              label, gain, *oldWeight, *finalScale, fixedScale);
                    *finalScale = fixedScale;
                }
            } else if (gain < 0.5f) {
                Log::Warn("%s gain %.3f < 0.5: heavily over-damped response.", label, gain);
            }
            if (gCfg.LogFilterGains)
                Log::Info("%s: OldWeight=%.3f FinalScale=%.4f gain=%.3f.",
                          label, *oldWeight, *finalScale,
                          (*oldWeight + 1.0f) * (*finalScale));
        }
    }

    void Sanitize() {
        // ---- ordering rules --------------------------------------------------
        if (gCfg.AccelBudgetMin > gCfg.AccelBudgetMax) {
            Log::Warn("AccelBudgetMin (%.1f) > AccelBudgetMax (%.1f); Min lowered to Max.",
                      gCfg.AccelBudgetMin, gCfg.AccelBudgetMax);
            gCfg.AccelBudgetMin = gCfg.AccelBudgetMax;
        }
        if (gCfg.MaxChaseSpeed > 0.0f && gCfg.MinChaseSpeed > gCfg.MaxChaseSpeed) {
            Log::Warn("MinChaseSpeed > MaxChaseSpeed; Min lowered to Max.");
            gCfg.MinChaseSpeed = gCfg.MaxChaseSpeed;
        }
        if (gCfg.DynAggrMin > gCfg.DynAggrMax) gCfg.DynAggrMin = gCfg.DynAggrMax;
        if (gCfg.MinCommandedHeight > gCfg.MaxCommandedHeight) {
            Log::Warn("MinCommandedHeight > MaxCommandedHeight; Min lowered to Max.");
            gCfg.MinCommandedHeight = gCfg.MaxCommandedHeight;
        }
        if (gCfg.LeadMin > gCfg.LeadMax) {
            Log::Warn("LeadMin > LeadMax; LeadMin lowered to LeadMax.");
            gCfg.LeadMin = gCfg.LeadMax;
        }
        if (gCfg.RecoveryReleaseDegrees >= gCfg.MaximumSafePitchDegrees) {
            Log::Warn("RecoveryReleaseDegrees >= MaximumSafePitchDegrees; lowered to %.1f.",
                      gCfg.MaximumSafePitchDegrees * 0.5f);
            gCfg.RecoveryReleaseDegrees = gCfg.MaximumSafePitchDegrees * 0.5f;
        }
        if (gCfg.Stage3Seconds <= gCfg.Stage2Seconds) {
            Log::Warn("Stage3Seconds <= Stage2Seconds; Stage3 raised to Stage2 + 1s.");
            gCfg.Stage3Seconds = gCfg.Stage2Seconds + 1.0f;
        }
        if (gCfg.SkidEntryMinDistance >= gCfg.SkidEntryMaxDistance)
            Log::Info("SkidEntryMinDistance >= SkidEntryMaxDistance: entry window empty, "
                      "attacks never start (follow-only via gates).");

        // ---- filter-gain safety (the anti-spin rule) ---------------------------
        if (gCfg.EnableSmoothingPatches && gCfg.PatchOutputSmoothing)
            EnforceFilterPair("Output smoothing filter",
                              &gCfg.SmoothingOldWeight, &gCfg.SmoothingFinalScale);
        if (gCfg.EnableSmoothingPatches && gCfg.PatchDestVelFilter)
            EnforceFilterPair("Destination-velocity filter",
                              &gCfg.DestVelFilterOldWeight, &gCfg.DestVelFilterFinalScale);

        // ---- dynamic-AI dependency auto-enables --------------------------------
        {
            const bool needsEntryGates =
                gCfg.FollowOnly || gCfg.AttackOnlyWhenAligned
                || gCfg.SuppressAttacksWhenStopped
                || gCfg.ReattackDelaySeconds > 0.0f
                || gCfg.EnableAggression || gCfg.EnableDynamicAggression;
            if (needsEntryGates && !gCfg.EnableSkidEntryPatches) {
                gCfg.EnableSkidEntryPatches = true;
                Log::Info("[Config] dynamic attack control requested: [SkidEntry] "
                          "Enable forced ON (its live values are the mechanism).");
            }
            const bool needsStrike =
                (gCfg.AbortAttackOnReversal && gCfg.EnableDirectionChange)
                || gCfg.MaxStrikeSeconds > 0.0f;
            if (needsStrike && !gCfg.EnableSkidStrikePatches) {
                gCfg.EnableSkidStrikePatches = true;
                Log::Info("[Config] attack aborts requested: [SkidStrike] Enable forced ON.");
            }
            const bool needsLead =
                gCfg.EnablePrediction || gCfg.EnableStationaryPlayer
                || gCfg.EnableDirectionChange || gCfg.EnableStuckRecovery
                || gCfg.EnableAntiCircling;
            if (needsLead && !gCfg.EnableLeadPatches) {
                gCfg.EnableLeadPatches = true;
                Log::Info("Chase-leading enabled automatically (required by another "
                          "enabled option).");
            }
            const bool needsAltitude =
                gCfg.EnableDynamicAltitude || gCfg.EnableStuckRecovery;
            if (needsAltitude && !gCfg.EnableAltitudePatches) {
                gCfg.EnableAltitudePatches = true;
                Log::Info("Altitude control enabled automatically (required by another "
                          "enabled option).");
            }
            // Holding the engine's motion filters at the reference rate means
            // owning those two constants, which is the smoothing patch group.
            // With vanilla values it is a no-op at the reference rate.
            if (gCfg.EnableFrameRateFix && !gCfg.EnableSmoothingPatches) {
                gCfg.EnableSmoothingPatches = true;
                Log::Info("Motion smoothing enabled automatically (needed to keep the "
                          "helicopter flying like it does at %.0f FPS).",
                          gCfg.HelicopterUpdateRate);
            }
        }

        // ---- advisories ------------------------------------------------------
        if (gCfg.ForceSkidHitAttribute)
            Log::Warn("ForceSkidHitAttribute is on: the helicopter may attack even at "
                      "low pursuit levels. Turn it off for more restrained behavior.");
        if (gCfg.EnableDispatchPatches)
            Log::Warn("The extra-helicopter spawner is enabled. This is an experimental "
                      "diagnostics option and may cause instability; leave it off for "
                      "normal play.");
        if (gCfg.BypassSpawnCapForImmediateRequests)
            Log::Warn("BypassSpawnCapForImmediateRequests affects all police spawns, "
                      "not only helicopters.");
        if (gCfg.FollowOnly)
            Log::Info("Follow-only mode is on: the helicopter tracks the player but "
                      "does not attack.");
        // Reported unconditionally: if GeneralSettings.ini predates this
        // version its [FrameRate] section is missing and these are built-in
        // defaults - this line is how you tell.
        if (gCfg.EnableFrameRateFix)
            Log::Info("Frame rate: the helicopter will fly like it does at %.0f FPS "
                      "whatever frame rate the game runs at.", gCfg.HelicopterUpdateRate);
        else
            Log::Info("Frame rate: compensation is off. Above %.0f FPS the helicopter "
                      "will sway less, and above 200 FPS it will stop moving properly.",
                      gCfg.HelicopterUpdateRate);
        if (gCfg.IgnoreHeliSheet) {
            Log::Warn("IgnoreHeliSheet is on: the terrain-height guidance is ignored "
                      "at all times. The helicopter can descend anywhere, and the "
                      "mod's attack-event detection and attack logging are inactive "
                      "while this is enabled.");
            if (gCfg.RespectHeliSheetDuringSkid)
                Log::Warn("RespectHeliSheetDuringSkid has no effect while "
                          "IgnoreHeliSheet is enabled.");
            if (gCfg.EnableSheetSafeOverride)
                Log::Info("The altitude safe-override is unnecessary while "
                          "IgnoreHeliSheet is enabled (the unconditional ignore "
                          "takes precedence).");
        }
    }

} // namespace Validation
