#pragma once

namespace AIVehicleHelicopter { struct Snapshot; }

namespace SimpleChopper {

    void InstallPatches();
    void Refresh();
    void ScaleMotionFilters(float delta);

    void BeginHelicopter();
    void ApplySpeedCap(const AIVehicleHelicopter::Snapshot& snapshot);
    void RestoreSpeedCap();

}
