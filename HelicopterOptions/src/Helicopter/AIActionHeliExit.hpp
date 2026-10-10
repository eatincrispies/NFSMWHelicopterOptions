#pragma once
#include "AIPerpVehicle.hpp"

class AIActionHeliExit {
  public:
    struct Settings {
        float FlySpeed = 100.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:FlySpeed", "FlySpeed", &sSettings.FlySpeed, nullptr, 40.0f, 220.0f, true },
    };

    static constexpr Patch::FloatPush kUpdateFlySpeed = { 0x00427BFEu, 100.0f };

    static inline bool  sPatched  = false;
    static inline float sFlySpeed = 0.0f;

    static void InstallPatches() {
        Patch::Begin("AIActionHeliExit::Update");
        Patch::PushFloat("flySpeed", kUpdateFlySpeed, sSettings.FlySpeed);
        sPatched = Patch::Commit();
        sFlySpeed = sSettings.FlySpeed;
    }

    static void Refresh();
};
