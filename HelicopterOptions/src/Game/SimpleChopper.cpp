#include <cmath>
#include <cstdint>
#include "SimpleChopper.h"
#include "AIVehicleHelicopter.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/FrameTime.h"
#include "../Core/Log.h"
#include "../Core/Memory.h"
#include "../Core/PatchManager.h"

namespace SimpleChopper {

    namespace {

        namespace Game = Addr::SimpleChopper;

        constexpr float kVelocityDeltaTimeGate = 0.0001f;
        constexpr float kGameBrakeFactor       = 0.4f;
        constexpr int   kMaxSpeedFields        = 4;

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
        float         gScaledRequest = 0.0f;

        bool FindMaxSpeed(void* heli, float** field, float* value) {
            void* chopper = nullptr;
            uint32_t table = 0;
            void* layout = nullptr;
            if (!heli
                || !Memory::Read(reinterpret_cast<uintptr_t>(heli) + Addr::AIVehicleHelicopter::ISimpleChopper, &chopper, sizeof(chopper))
                || !chopper
                || !Memory::Read(reinterpret_cast<uintptr_t>(chopper), &table, sizeof(table))
                || table != Game::InterfaceVtable
                || !Memory::Read(reinterpret_cast<uintptr_t>(chopper) + Game::ChopperSpecsLayout, &layout, sizeof(layout))
                || !layout)
                return false;

            const uintptr_t address = reinterpret_cast<uintptr_t>(layout) + Addr::chopperspecs::MAX_SPEED_MPS;
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
            Memory::WriteData(Game::Max_Chopper_Accel, &gCfg.MaxChopperAccel, sizeof(float));
            Memory::WriteData(Game::Min_Chopper_Accel, &gCfg.MinChopperAccel, sizeof(float));
            gMaxChopperAccel = gCfg.MaxChopperAccel;
            gMinChopperAccel = gCfg.MinChopperAccel;
        }
    }

    void InstallPatches() {
        Refresh();
        gLive.smoothingOldWeight    = Game::Vanilla::SmoothingOldWeight;
        gLive.smoothingFinalScale   = Game::Vanilla::SmoothingFinalScale;
        gLive.destinationOldWeight  = Game::Vanilla::DestinationOldWeight;
        gLive.destinationFinalScale = Game::Vanilla::DestinationFinalScale;
        gLive.velocityDeltaTimeGate = kVelocityDeltaTimeGate;

        Patch::Begin("SimpleChopper::OnTaskSimulate");
        Patch::RedirectFloat("TurnResponseScale", Game::TurnResponseScale, &gLive.turnResponseScale);
        Patch::RedirectFloat("TurnClamp compare", Game::TurnClampCompare, &gLive.turnClamp);
        Patch::RedirectFloat("TurnClamp load", Game::TurnClampLoad, &gLive.turnClamp);
        Patch::RedirectFloat("TurnClamp negative", Game::TurnClampNegativeA, &gLive.turnClampNegative);
        Patch::RedirectFloat("TurnClamp negative", Game::TurnClampNegativeB, &gLive.turnClampNegative);
        for (const Addr::FloatOperand& site : Game::SmoothingOldWeight)
            Patch::RedirectFloat("smoothing weight", site, &gLive.smoothingOldWeight);
        for (const Addr::FloatOperand& site : Game::SmoothingFinalScale)
            Patch::RedirectFloat("smoothing scale", site, &gLive.smoothingFinalScale);
        Patch::RedirectFloat("velocity delta-time gate", Game::VelocityDeltaTimeGate, &gLive.velocityDeltaTimeGate);
        Patch::Commit();

        Patch::Begin("SimpleChopper::OnTaskSimulate destination velocity filter");
        for (const Addr::FloatOperand& site : Game::DestinationOldWeight)
            Patch::RedirectFloat("destination weight", site, &gLive.destinationOldWeight);
        for (const Addr::FloatOperand& site : Game::DestinationFinalScale)
            Patch::RedirectFloat("destination scale", site, &gLive.destinationFinalScale);
        Patch::Commit();

        Patch::Begin("Max_Chopper_Accel and Min_Chopper_Accel");
        Patch::DataFloat("Max_Chopper_Accel", Game::Max_Chopper_Accel, Game::Vanilla::MaxChopperAccel, gCfg.MaxChopperAccel);
        Patch::DataFloat("Min_Chopper_Accel", Game::Min_Chopper_Accel, Game::Vanilla::MinChopperAccel, gCfg.MinChopperAccel);
        gAccelPatched = Patch::Commit();
        gMaxChopperAccel = gCfg.MaxChopperAccel;
        gMinChopperAccel = gCfg.MinChopperAccel;
    }

    void ScaleMotionFilters(float delta) {
        FrameTime::ScaleFilterPair(Game::Vanilla::SmoothingOldWeight, Game::Vanilla::SmoothingFinalScale, delta,
                                   &gLive.smoothingOldWeight, &gLive.smoothingFinalScale);
        FrameTime::ScaleFilterPair(Game::Vanilla::DestinationOldWeight, Game::Vanilla::DestinationFinalScale, delta,
                                   &gLive.destinationOldWeight, &gLive.destinationFinalScale);
    }

    void BeginHelicopter() {
        gScaledRequest = 0.0f;
    }

    void ApplySpeedCap(const AIVehicleHelicopter::Snapshot& s) {
        float* field = nullptr;
        float current = 0.0f;
        if (!FindMaxSpeed(s.heli, &field, &current)) return;
        const MaxSpeedField* tracked = TrackMaxSpeed(field, current);
        if (!tracked) return;

        if (current != gCfg.SpeedCap && WriteMaxSpeed(field, gCfg.SpeedCap))
            Log::Info("chopperspecs MAX_SPEED_MPS set to %g m/s (%.0f km/h); the game's value is %g m/s.",
                      gCfg.SpeedCap, gCfg.SpeedCap * 3.6f, tracked->original);

        if (gCfg.SpeedCap == tracked->original || s.driveSpeed <= 0.0f) return;

        const bool alreadyScaled = gScaledRequest > 0.0f
            && (s.driveSpeed == gScaledRequest
                || std::fabs(s.driveSpeed - gScaledRequest * kGameBrakeFactor) <= gScaledRequest * 0.001f);
        if (alreadyScaled) return;

        gScaledRequest = s.driveSpeed * gCfg.SpeedCap / tracked->original;
        AIVehicleHelicopter::WriteDriveSpeed(s, gScaledRequest);
    }

    void RestoreSpeedCap() {
        for (int i = 0; i < gMaxSpeedFieldCount; ++i)
            WriteMaxSpeed(gMaxSpeedFields[i].field, gMaxSpeedFields[i].original);
        gMaxSpeedFieldCount = 0;
    }

}
