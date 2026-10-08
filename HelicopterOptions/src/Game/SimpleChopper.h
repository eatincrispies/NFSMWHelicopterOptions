#pragma once

namespace AIVehicleHelicopter { struct Snapshot; }

namespace SimpleChopper {

    void InstallPatches();
    void Refresh();
    void ScaleMotionFilters(float frames);

    void ApplySpeedCap(const AIVehicleHelicopter::Snapshot& snapshot);
    void RestoreSpeedCap();

}
