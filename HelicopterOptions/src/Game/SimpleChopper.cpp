#include <algorithm>
#include <cmath>
#include <cstdint>
#include "SimpleChopper.h"
#include "AIVehicleHelicopter.h"
#include "Interfaces.h"
#include "../Config/Config.h"

namespace SimpleChopper {

    namespace {

        constexpr uintptr_t kInterfaceVtable    = 0x008AB86Cu;
        constexpr unsigned  kChopperSpecsLayout = 0x58u;
        constexpr unsigned  kMaxSpeedMps        = 0x44u;

        constexpr Patch::FloatOperand kTurnResponseScale  = { 0x006A28A8u, { 0xD8, 0x0D }, 0x008AB8ECu };
        constexpr Patch::FloatOperand kTurnClampCompare   = { 0x006A28BAu, { 0xD8, 0x1D }, 0x008AAE5Cu };
        constexpr Patch::FloatOperand kTurnClampLoad      = { 0x006A2994u, { 0xD9, 0x05 }, 0x008AAE5Cu };
        constexpr Patch::FloatOperand kTurnClampNegativeA = { 0x006A28CFu, { 0xD9, 0x05 }, 0x008AB8E8u };
        constexpr Patch::FloatOperand kTurnClampNegativeB = { 0x006A28E2u, { 0xD9, 0x05 }, 0x008AB8E8u };

        constexpr Patch::FloatOperand kSmoothingOldWeight[3] = {
            { 0x006A28EFu, { 0xD8, 0x0D }, 0x008A0718u },
            { 0x006A28FCu, { 0xD8, 0x0D }, 0x008A0718u },
            { 0x006A2908u, { 0xD8, 0x0D }, 0x008A0718u },
        };
        constexpr Patch::FloatOperand kSmoothingFinalScale[3] = {
            { 0x006A291Eu, { 0xD8, 0x0D }, 0x00890F14u },
            { 0x006A292Bu, { 0xD8, 0x0D }, 0x00890F14u },
            { 0x006A293Cu, { 0xD8, 0x0D }, 0x00890F14u },
        };
        constexpr Patch::FloatOperand kDestinationOldWeight[3] = {
            { 0x006A204Fu, { 0xD8, 0x0D }, 0x00890E98u },
            { 0x006A205Bu, { 0xD8, 0x0D }, 0x00890E98u },
            { 0x006A2067u, { 0xD8, 0x0D }, 0x00890E98u },
        };
        constexpr Patch::FloatOperand kDestinationFinalScale[3] = {
            { 0x006A2075u, { 0xD8, 0x0D }, 0x00895074u },
            { 0x006A2083u, { 0xD8, 0x0D }, 0x00895074u },
            { 0x006A2092u, { 0xD8, 0x0D }, 0x00895074u },
        };
        constexpr Patch::FloatOperand kVelocityDeltaTimeGate = { 0x006A2762u, { 0xD8, 0x1D }, 0x00890EC4u };

        constexpr uintptr_t kMaxChopperAccel = 0x008F8DCCu;
        constexpr uintptr_t kMinChopperAccel = 0x008F8DD0u;

        constexpr float kGameSmoothingOldWeight    = 7.0f;
        constexpr float kGameSmoothingFinalScale   = 0.125f;
        constexpr float kGameDestinationOldWeight  = 4.0f;
        constexpr float kGameDestinationFinalScale = 0.2f;
        constexpr float kGameMaxChopperAccel       = 80.0f;
        constexpr float kGameMinChopperAccel       = 30.0f;

        constexpr float kVelocityDeltaTimeGateValue = 0.0001f;
        constexpr int   kMaxSpeedFields             = 4;

        struct LiveValues {
            float turnResponseScale;
            float turnClamp;
            float turnClampNegative;
            float smoothingOldWeight;
            float smoothingFinalScale;
            float destinationOldWeight;
            float destinationFinalScale;
            float velocityDeltaTimeGate;
        };

        struct MaxSpeedField {
            float* field;
            float  original;
        };

        LiveValues    gLive = {};
        bool          gAccelPatched = false;
        float         gMaxChopperAccel = 0.0f;
        float         gMinChopperAccel = 0.0f;
        MaxSpeedField gMaxSpeedFields[kMaxSpeedFields] = {};
        int           gMaxSpeedFieldCount = 0;

        void ScaleFilterPair(float weight, float scale, float frames, float* outWeight, float* outScale) {
            *outWeight = weight;
            *outScale = scale;

            const float retained = weight * scale;
            const float steps = std::isfinite(frames) ? std::clamp(frames, 0.0f, 64.0f) : 1.0f;
            if (!(retained > 0.0f) || retained >= 1.0f || steps <= 0.0f || steps == 1.0f) return;

            const float retainedNow = std::pow(retained, steps);
            if (!std::isfinite(retainedNow) || retainedNow <= 0.0f || retainedNow >= 0.999999f) return;

            const float newScale = 1.0f - retainedNow;
            *outScale = newScale;
            *outWeight = retainedNow / newScale;
        }

        bool FindMaxSpeed(void* chopper, float** field, float* value) {
            uint32_t table = 0;
            void* layout = nullptr;
            if (!chopper
                || !Memory::Read(reinterpret_cast<uintptr_t>(chopper), &table, sizeof(table))
                || table != kInterfaceVtable
                || !Memory::Read(reinterpret_cast<uintptr_t>(chopper) + kChopperSpecsLayout, &layout, sizeof(layout))
                || !layout)
                return false;

            const uintptr_t address = reinterpret_cast<uintptr_t>(layout) + kMaxSpeedMps;
            float speed = 0.0f;
            if (!Memory::Read(address, &speed, sizeof(speed)) || !Memory::IsFinite(speed) || speed < 1.0f || speed > 10000.0f)
                return false;

            *field = reinterpret_cast<float*>(address);
            *value = speed;
            return true;
        }

        bool WriteMaxSpeed(float* field, float value) {
            return field && Memory::IsFinite(value)
                && Memory::WriteData(reinterpret_cast<uintptr_t>(field), &value, sizeof(value));
        }

        const MaxSpeedField* TrackMaxSpeed(float* field, float current) {
            for (int i = 0; i < gMaxSpeedFieldCount; ++i)
                if (gMaxSpeedFields[i].field == field) return &gMaxSpeedFields[i];
            if (gMaxSpeedFieldCount == kMaxSpeedFields) return nullptr;
            gMaxSpeedFields[gMaxSpeedFieldCount] = { field, current };
            return &gMaxSpeedFields[gMaxSpeedFieldCount++];
        }

    }

    void Refresh() {
        gLive.turnResponseScale = gCfg.TurnResponseScale;
        gLive.turnClamp         = gCfg.TurnClamp;
        gLive.turnClampNegative = -gCfg.TurnClamp;

        if (gAccelPatched && (gCfg.MaxChopperAccel != gMaxChopperAccel || gCfg.MinChopperAccel != gMinChopperAccel)) {
            Memory::WriteData(kMaxChopperAccel, &gCfg.MaxChopperAccel, sizeof(float));
            Memory::WriteData(kMinChopperAccel, &gCfg.MinChopperAccel, sizeof(float));
            gMaxChopperAccel = gCfg.MaxChopperAccel;
            gMinChopperAccel = gCfg.MinChopperAccel;
        }
    }

    void InstallPatches() {
        Refresh();
        gLive.smoothingOldWeight    = kGameSmoothingOldWeight;
        gLive.smoothingFinalScale   = kGameSmoothingFinalScale;
        gLive.destinationOldWeight  = kGameDestinationOldWeight;
        gLive.destinationFinalScale = kGameDestinationFinalScale;
        gLive.velocityDeltaTimeGate = kVelocityDeltaTimeGateValue;

        Patch::Begin("SimpleChopper::OnTaskSimulate");
        Patch::RedirectFloat("TurnResponseScale", kTurnResponseScale, &gLive.turnResponseScale);
        Patch::RedirectFloat("TurnClamp compare", kTurnClampCompare, &gLive.turnClamp);
        Patch::RedirectFloat("TurnClamp load", kTurnClampLoad, &gLive.turnClamp);
        Patch::RedirectFloat("TurnClamp negative", kTurnClampNegativeA, &gLive.turnClampNegative);
        Patch::RedirectFloat("TurnClamp negative", kTurnClampNegativeB, &gLive.turnClampNegative);
        for (const Patch::FloatOperand& site : kSmoothingOldWeight)
            Patch::RedirectFloat("smoothing weight", site, &gLive.smoothingOldWeight);
        for (const Patch::FloatOperand& site : kSmoothingFinalScale)
            Patch::RedirectFloat("smoothing scale", site, &gLive.smoothingFinalScale);
        Patch::RedirectFloat("velocity delta-time gate", kVelocityDeltaTimeGate, &gLive.velocityDeltaTimeGate);
        Patch::Commit();

        Patch::Begin("SimpleChopper::OnTaskSimulate destination velocity filter");
        for (const Patch::FloatOperand& site : kDestinationOldWeight)
            Patch::RedirectFloat("destination weight", site, &gLive.destinationOldWeight);
        for (const Patch::FloatOperand& site : kDestinationFinalScale)
            Patch::RedirectFloat("destination scale", site, &gLive.destinationFinalScale);
        Patch::Commit();

        Patch::Begin("Max_Chopper_Accel and Min_Chopper_Accel");
        Patch::DataFloat("Max_Chopper_Accel", kMaxChopperAccel, kGameMaxChopperAccel, gCfg.MaxChopperAccel);
        Patch::DataFloat("Min_Chopper_Accel", kMinChopperAccel, kGameMinChopperAccel, gCfg.MinChopperAccel);
        gAccelPatched = Patch::Commit();
        gMaxChopperAccel = gCfg.MaxChopperAccel;
        gMinChopperAccel = gCfg.MinChopperAccel;
    }

    void ScaleMotionFilters(float frames) {
        ScaleFilterPair(kGameSmoothingOldWeight, kGameSmoothingFinalScale, frames,
                        &gLive.smoothingOldWeight, &gLive.smoothingFinalScale);
        ScaleFilterPair(kGameDestinationOldWeight, kGameDestinationFinalScale, frames,
                        &gLive.destinationOldWeight, &gLive.destinationFinalScale);
    }

    void ApplySpeedCap(const AIVehicleHelicopter::Snapshot& snapshot) {
        float* field = nullptr;
        float current = 0.0f;
        if (!FindMaxSpeed(snapshot.chopper, &field, &current)) return;
        const MaxSpeedField* tracked = TrackMaxSpeed(field, current);
        if (!tracked || current == gCfg.SpeedCap || !WriteMaxSpeed(field, gCfg.SpeedCap)) return;

        Log::Info("chopperspecs MAX_SPEED_MPS set to %g m/s (%.0f km/h); the game's value is %g m/s.",
                  gCfg.SpeedCap, gCfg.SpeedCap * 3.6f, tracked->original);
    }

    void RestoreSpeedCap() {
        for (int i = 0; i < gMaxSpeedFieldCount; ++i)
            WriteMaxSpeed(gMaxSpeedFields[i].field, gMaxSpeedFields[i].original);
        gMaxSpeedFieldCount = 0;
    }

}
