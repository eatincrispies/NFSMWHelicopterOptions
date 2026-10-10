#pragma once
#include "AIPerpVehicle.hpp"

class AIActionHeliPursuit;

namespace HeliSheet {

    struct Settings {
        bool  HeliSheet               = true;
        float IgnoreHeliSheetDistance = 0.0f;
    };

    inline Settings sSettings;

    inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:HeliSheet",               "HeliSheet",               nullptr,                            &sSettings.HeliSheet, 0.0f, 1.0f,    false },
        { "Helicopter:IgnoreHeliSheetDistance", "IgnoreHeliSheetDistance", &sSettings.IgnoreHeliSheetDistance, nullptr,              0.0f, 2000.0f, false },
    };

    void BeginHelicopter();
    void Update(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit);

}
