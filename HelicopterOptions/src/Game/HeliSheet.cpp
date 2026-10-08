#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include "HeliSheet.h"
#include "AIVehicleHelicopter.h"
#include "Interfaces.h"
#include "../Config/Config.h"

namespace HeliSheet {

    namespace {

        constexpr uintptr_t kIgnoreHeliSheet      = 0x0090D621u;
        constexpr uintptr_t kNeverIgnoreHeliSheet = 0x008EB1F4u;
        constexpr int       kAttackMode           = 2;
        constexpr float     kReturnFraction       = 0.8f;

        bool          gFarFromTarget = false;
        bool          gIgnoring = false;
        unsigned long gLastLogMs = 0;

        bool GameIgnoresDuringAttack(int mode) {
            uint8_t never = 1;
            Memory::Read(kNeverIgnoreHeliSheet, &never, sizeof(never));
            return mode >= kAttackMode && never == 0;
        }

#if defined(_DEBUG)
        unsigned long gLastHeightLogMs = 0;

        void LogHeight(const AIVehicleHelicopter::Snapshot& s) {
            const unsigned long now = GetTickCount();
            float playerPosition[3];
            if (now - gLastHeightLogMs < 2000 || !Interfaces::ReadPlayerPosition(playerPosition)) return;
            gLastHeightLogMs = now;

            uint8_t ignored = 0;
            Memory::Read(kIgnoreHeliSheet, &ignored, sizeof(ignored));
            Log::Info("Helicopter is %.0f m above you (attack mode %d); the heli sheet is %s.",
                      s.position[1] - playerPosition[1], s.mode, ignored ? "ignored" : "obeyed");
        }
#endif

    }

    void BeginHelicopter() {
        gFarFromTarget = false;
    }

    void Update(const AIVehicleHelicopter::Snapshot& s) {
        bool outOfRange = false;
        float playerPosition[3];
        if (gCfg.IgnoreHeliSheetDistance > 0.0f && Interfaces::ReadPlayerPosition(playerPosition)) {
            const float dx = s.position[0] - playerPosition[0];
            const float dz = s.position[2] - playerPosition[2];
            const float distance = std::sqrt(dx * dx + dz * dz);
            const float returnDistance = gCfg.IgnoreHeliSheetDistance * kReturnFraction;
            outOfRange = distance > (gFarFromTarget ? returnDistance : gCfg.IgnoreHeliSheetDistance);

            const unsigned long now = GetTickCount();
            if (outOfRange != gFarFromTarget && now - gLastLogMs >= 10000) {
                gLastLogMs = now;
                if (outOfRange)
                    Log::Info("Helicopter is %.0f m away: ignoring the heli sheet until it is back within %.0f m.",
                              distance, returnDistance);
                else
                    Log::Info("Helicopter is back within %.0f m: obeying the heli sheet again.", returnDistance);
            }
        }
        gFarFromTarget = outOfRange;

        const bool ignore = !gCfg.HeliSheet || outOfRange;
        if (ignore || gIgnoring) {
            const uint8_t flag = (ignore || GameIgnoresDuringAttack(s.mode)) ? 1 : 0;
            Memory::WriteData(kIgnoreHeliSheet, &flag, sizeof(flag));
            gIgnoring = ignore;
        }

#if defined(_DEBUG)
        LogHeight(s);
#endif
    }

}
