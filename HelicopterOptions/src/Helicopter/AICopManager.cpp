#include "AICopManager.hpp"

void AICopManager::Refresh() {
    if (!sPatched || sSettings.SpawnDistance == sSpawnDistance) return;
    if (!Patch::RewritePushedFloat(kSpawnPursuitHelicopterIncNavPosition, sSpawnDistance, sSettings.SpawnDistance)) {
        sPatched = false;
        Log::Warn("Another mod changed AICopManager::SpawnPursuitHelicopter, so [Helicopter:SpawnDistance] stops updating.");
        return;
    }
    sSpawnDistance = sSettings.SpawnDistance;
}
