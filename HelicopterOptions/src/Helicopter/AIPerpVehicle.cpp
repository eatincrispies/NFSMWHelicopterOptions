#include <cstddef>
#include <cstdint>
#include "AIPerpVehicle.hpp"
#include "AIActionHeliExit.hpp"
#include "AIActionHeliPursuit.hpp"
#include "AICopManager.hpp"
#include "Hooks.hpp"
#include "SimpleChopper.hpp"

namespace {

    constexpr uintptr_t kSetHeat            = 0x00409060u;
    constexpr uint8_t   kSetHeatPrologue[7] = { 0x6A, 0xFF, 0x68, 0xBC, 0x79, 0x86, 0x00 };
    constexpr uint32_t  kSetHeatArgument    = 0x08u;

    static_assert(offsetof(AIPerpVehicle, mIOwner) == 0x34, "Behavior::mIOwner");
    static_assert(offsetof(AIPerpVehicle, mIPerpetrator) == 0x758, "AIPerpVehicle's IPerpetrator");
    static_assert(offsetof(AIPerpVehicle, mHeat) == 0x774, "AIPerpVehicle::mHeat");
    static_assert(offsetof(AIPerpVehicle, mWasInRaceEventLastHeatUpdate) == 0x786, "AIPerpVehicle::mWasInRaceEventLastHeatUpdate");

    AIPerpVehicle* gPlayerPerp = nullptr;
    uintptr_t      gPlayerPerpVTable = 0;

    bool IsHeatLevel(float heat) {
        return heat >= 1.0f && heat < 11.0f;
    }

    AIPerpVehicle* FromIPerpetrator(uintptr_t iperp) {
        return iperp ? reinterpret_cast<AIPerpVehicle*>(iperp - offsetof(AIPerpVehicle, mIPerpetrator)) : nullptr;
    }

    uintptr_t IPerpetratorVTable(const AIPerpVehicle* perp) {
        uintptr_t vtable = 0;
        Memory::Read(reinterpret_cast<uintptr_t>(perp) + offsetof(AIPerpVehicle, mIPerpetrator), &vtable, sizeof(vtable));
        return vtable;
    }

    void ApplyHeat(int level, bool racing) {
        if (!Ini::ApplyHeat(level, racing)) return;
        AIActionHeliPursuit::Refresh();
        SimpleChopper::Refresh();
        AIActionHeliExit::Refresh();
        AICopManager::Refresh();
    }

    void __cdecl SetHeatEntry(Detour::Registers* registers) {
        AIPerpVehicle* perp = FromIPerpetrator(registers->ecx);
        if (!perp || !perp->IsLocalPlayer()) return;

        if (perp != gPlayerPerp) {
            gPlayerPerp = perp;
            Log::Info("Heat now follows your car (AIPerpVehicle %p).", static_cast<void*>(perp));
        }
        gPlayerPerpVTable = IPerpetratorVTable(perp);

        float heat = 0.0f;
        if (!Memory::Read(registers->esp + kSetHeatArgument, &heat, sizeof(heat)) || !IsHeatLevel(heat)) return;
        ApplyHeat(static_cast<int>(heat), perp->mWasInRaceEventLastHeatUpdate);
    }

}

bool AIPerpVehicle::HookSetHeat() {
    return Detour::Install("AIPerpVehicle::SetHeat", kSetHeat, kSetHeatPrologue, sizeof(kSetHeatPrologue), &SetHeatEntry);
}

void AIPerpVehicle::UpdateHeat() {
    if (!gPlayerPerp || IPerpetratorVTable(gPlayerPerp) != gPlayerPerpVTable) return;

    float   heat = 0.0f;
    uint8_t racing = 0;
    const uintptr_t va = reinterpret_cast<uintptr_t>(gPlayerPerp);
    if (!Memory::Read(va + offsetof(AIPerpVehicle, mHeat), &heat, sizeof(heat)) || !IsHeatLevel(heat)
        || !Memory::Read(va + offsetof(AIPerpVehicle, mWasInRaceEventLastHeatUpdate), &racing, sizeof(racing)))
        return;
    ApplyHeat(static_cast<int>(heat), racing != 0);
}

bool AIPerpVehicle::IsLocalPlayer() const {
    IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
    return mIOwner != nullptr && playerRigidBody != nullptr && mIOwner->GetRigidBody() == playerRigidBody;
}
