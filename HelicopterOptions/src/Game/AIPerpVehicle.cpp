#include <cstdint>
#include "AIPerpVehicle.h"
#include "AIActionHeliExit.h"
#include "AIActionHeliPursuit.h"
#include "AICopManager.h"
#include "Interfaces.h"
#include "SimpleChopper.h"
#include "../Config/Ini.h"

namespace AIPerpVehicle {

    namespace {

        constexpr uintptr_t kSetHeat            = 0x00409060u;
        constexpr uint8_t   kSetHeatPrologue[7] = { 0x6A, 0xFF, 0x68, 0xBC, 0x79, 0x86, 0x00 };
        constexpr unsigned  kOwner              = 0x34u;
        constexpr unsigned  kIPerpetrator       = 0x758u;
        constexpr unsigned  kHeat               = 0x1Cu;
        constexpr unsigned  kIsRacing           = 0x2Eu;

        void*    gPerp = nullptr;
        uint32_t gPerpTable = 0;

        uintptr_t Field(void* perp, unsigned offset) {
            return reinterpret_cast<uintptr_t>(perp) + offset;
        }

        bool ValidHeat(float heat) {
            return heat >= 1.0f && heat < 11.0f;
        }

        bool IsPlayer(void* perp) {
            void* owner = nullptr;
            void* rigidBody = nullptr;
            void* playerRigidBody = nullptr;
            return perp
                && Memory::Read(Field(perp, 0) - kIPerpetrator + kOwner, &owner, sizeof(owner)) && owner
                && Interfaces::ReadRigidBody(owner, &rigidBody)
                && Interfaces::ReadPlayerRigidBody(&playerRigidBody)
                && rigidBody == playerRigidBody;
        }

        bool ReadRacing(void* perp, bool* racing) {
            uint8_t flag = 0;
            if (!Memory::Read(Field(perp, kIsRacing), &flag, sizeof(flag))) return false;
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
            void* perp = reinterpret_cast<void*>(static_cast<uintptr_t>(registers->ecx));
            if (!IsPlayer(perp)) return;

            if (perp != gPerp) {
                gPerp = perp;
                Log::Info("Heat now follows your car (AIPerpVehicle %p).", reinterpret_cast<void*>(Field(perp, 0) - kIPerpetrator));
            }
            Memory::Read(Field(perp, 0), &gPerpTable, sizeof(gPerpTable));

            float heat = 0.0f;
            if (!Memory::Read(registers->esp + 8u, &heat, sizeof(heat)) || !ValidHeat(heat)) return;
            bool racing = false;
            ReadRacing(perp, &racing);
            ApplyHeat(static_cast<int>(heat), racing);
        }

    }

    bool HookSetHeat() {
        return Detour::Install("AIPerpVehicle::SetHeat", kSetHeat, kSetHeatPrologue, sizeof(kSetHeatPrologue), &SetHeatEntry);
    }

    void UpdateHeat() {
        uint32_t table = 0;
        float heat = 0.0f;
        bool racing = false;
        if (!gPerp || !Memory::Read(Field(gPerp, 0), &table, sizeof(table)) || table != gPerpTable
            || !Memory::Read(Field(gPerp, kHeat), &heat, sizeof(heat)) || !ValidHeat(heat)
            || !ReadRacing(gPerp, &racing))
            return;
        ApplyHeat(static_cast<int>(heat), racing);
    }

}
