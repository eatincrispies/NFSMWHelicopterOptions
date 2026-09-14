#include <cstdint>
#include "AIPerpVehicle.h"
#include "AIActionHeliExit.h"
#include "AIActionHeliPursuit.h"
#include "AICopManager.h"
#include "SimpleChopper.h"
#include "../Config/Ini.h"
#include "../Core/Addresses.h"
#include "../Core/Detour.h"
#include "../Core/Memory.h"

namespace AIPerpVehicle {

    namespace {

        namespace Game = Addr::AIPerpVehicle;

        void* gPerp = nullptr;

        bool ValidHeat(float heat) {
            return heat >= 1.0f && heat < 11.0f;
        }

        bool ReadRacing(void* perp, bool* racing) {
            uint8_t flag = 0;
            if (!perp || !Memory::Read(reinterpret_cast<uintptr_t>(perp) + Game::IsRacing, &flag, sizeof(flag)))
                return false;
            *racing = flag != 0;
            return true;
        }

        void ApplyHeat(int level, bool racing) {
            if (!Ini::ApplyHeat(level, racing)) return;
            AIActionHeliPursuit::Refresh();
            SimpleChopper::Refresh();
            AIActionHeliExit::Refresh();
            AICopManager::Refresh();
        }

        void __cdecl SetHeatEntry(Detour::Registers* registers) {
            gPerp = reinterpret_cast<void*>(static_cast<uintptr_t>(registers->ecx));
            float heat = 0.0f;
            if (!Memory::Read(registers->esp + 8u, &heat, sizeof(heat)) || !ValidHeat(heat)) return;
            bool racing = false;
            ReadRacing(gPerp, &racing);
            ApplyHeat(static_cast<int>(heat), racing);
        }

    }

    bool HookSetHeat() {
        return Detour::Install("AIPerpVehicle::SetHeat", Game::SetHeat, Game::SetHeatPrologue,
                               sizeof(Game::SetHeatPrologue), &SetHeatEntry);
    }

    void UpdateHeat() {
        float heat = 0.0f;
        bool racing = false;
        if (!gPerp || !Memory::Read(reinterpret_cast<uintptr_t>(gPerp) + Game::Heat, &heat, sizeof(heat))
            || !ValidHeat(heat) || !ReadRacing(gPerp, &racing))
            return;
        ApplyHeat(static_cast<int>(heat), racing);
    }

}
