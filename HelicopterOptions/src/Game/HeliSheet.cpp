#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include "HeliSheet.h"
#include "AIVehicleHelicopter.h"
#include "Interfaces.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/Log.h"
#include "../Core/Memory.h"

namespace HeliSheet {

    namespace {

        constexpr float kReturnFraction = 0.8f;

        bool          gFarFromTarget = false;
        bool          gIgnoring = false;
        unsigned long gLastLogMs = 0;

        bool GameIgnoresDuringAttack(int mode) {
            uint8_t never = 1;
            Memory::Read(Addr::NeverIgnoreHeliSheet, &never, sizeof(never));
            return mode >= 2 && never == 0;
        }

    }

    void BeginHelicopter() {
        gFarFromTarget = false;
    }

    void Update(const AIVehicleHelicopter::Snapshot& s) {
        bool outOfRange = false;
        float playerPosition[3];
        float playerVelocity[3];
        if (gCfg.IgnoreHeliSheetDistance > 0.0f && Interfaces::ReadPlayer(playerPosition, playerVelocity)) {
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
        if (!ignore && !gIgnoring) return;

        const uint8_t flag = (ignore || GameIgnoresDuringAttack(s.mode)) ? 1 : 0;
        Memory::WriteData(Addr::bIgnoreHeliSheet, &flag, sizeof(flag));
        gIgnoring = ignore;
    }

}
