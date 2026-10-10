#pragma once
#include "AIPerpVehicle.hpp"

class AICopManager {
  public:
    struct Settings {
        float SpawnDistance = 250.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:SpawnDistance", "SpawnDistance", &sSettings.SpawnDistance, nullptr, 30.0f, 600.0f, true },
    };

    static constexpr Patch::FloatPush kSpawnPursuitHelicopterIncNavPosition = { 0x00426ABFu, 250.0f };

    static inline bool  sPatched       = false;
    static inline float sSpawnDistance = 0.0f;

    static void InstallPatches() {
        Patch::Begin("AICopManager::SpawnPursuitHelicopter");
        Patch::PushFloat("testNav.IncNavPosition distance", kSpawnPursuitHelicopterIncNavPosition, sSettings.SpawnDistance);
        sPatched = Patch::Commit();
        sSpawnDistance = sSettings.SpawnDistance;
    }

    static void Refresh();
};
