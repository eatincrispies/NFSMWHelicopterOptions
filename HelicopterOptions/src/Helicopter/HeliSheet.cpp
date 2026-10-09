#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "HeliSheet.hpp"
#include "AIActionHeliPursuit.hpp"
#include "Hooks.hpp"

namespace {

    constexpr uintptr_t kbIgnoreHeliSheet     = 0x0090D621u;
    constexpr uintptr_t kNeverIgnoreHeliSheet = 0x008EB1F4u;
    constexpr float     kReturnFraction       = 0.8f;

    bool          gFarFromTarget = false;
    bool          gIgnoring = false;
    unsigned long gLastRangeLogMs = 0;

    bool& bIgnoreHeliSheet() {
        return Game::Global<bool>(kbIgnoreHeliSheet);
    }

    bool NeverIgnoreHeliSheet() {
        return Game::Global<bool>(kNeverIgnoreHeliSheet);
    }

    bool GameIgnoresDuringSkidHit(const AIActionHeliPursuit* pursuit) {
        return pursuit != nullptr && pursuit->IsSkidHitting() && !NeverIgnoreHeliSheet();
    }

    bool IsOutOfRange(const UMath::Vector3& heliPosition) {
        IRigidBody* playerRigidBody = gCfg.IgnoreHeliSheetDistance > 0.0f ? GetLocalPlayerRigidBody() : nullptr;
        if (playerRigidBody == nullptr) return false;

        const float distance = UMath::Distancexz(heliPosition, playerRigidBody->GetPosition());
        const float returnDistance = gCfg.IgnoreHeliSheetDistance * kReturnFraction;
        const bool outOfRange = distance > (gFarFromTarget ? returnDistance : gCfg.IgnoreHeliSheetDistance);

        const unsigned long now = GetTickCount();
        if (outOfRange != gFarFromTarget && now - gLastRangeLogMs >= 10000) {
            gLastRangeLogMs = now;
            if (outOfRange)
                Log::Info("Helicopter is %.0f m away: ignoring the heli sheet until it is back within %.0f m.", distance, returnDistance);
            else
                Log::Info("Helicopter is back within %.0f m: obeying the heli sheet again.", returnDistance);
        }
        return outOfRange;
    }

#if defined(_DEBUG)
    unsigned long gLastHeightLogMs = 0;

    void LogHeight(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit) {
        const unsigned long now = GetTickCount();
        IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
        if (now - gLastHeightLogMs < 2000 || playerRigidBody == nullptr) return;
        gLastHeightLogMs = now;

        Log::Info("Helicopter is %.0f m above you (pursuit mode %d); the heli sheet is %s.", heliPosition.y - playerRigidBody->GetPosition().y,
                  pursuit != nullptr ? static_cast<int>(pursuit->mPursuitMode) : -1, bIgnoreHeliSheet() ? "ignored" : "obeyed");
    }
#else
    void LogHeight(const UMath::Vector3&, const AIActionHeliPursuit*) {}
#endif

}

void HeliSheet::BeginHelicopter() {
    gFarFromTarget = false;
}

void HeliSheet::Update(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit) {
    gFarFromTarget = IsOutOfRange(heliPosition);

    const bool ignore = !gCfg.HeliSheet || gFarFromTarget;
    if (ignore || gIgnoring) {
        bIgnoreHeliSheet() = ignore || GameIgnoresDuringSkidHit(pursuit);
        gIgnoring = ignore;
    }

    LogHeight(heliPosition, pursuit);
}
