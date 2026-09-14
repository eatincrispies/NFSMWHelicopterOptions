#pragma once

namespace AIVehicleHelicopter { struct Snapshot; }

namespace HeliSheet {

    void BeginHelicopter();
    void Update(const AIVehicleHelicopter::Snapshot& snapshot);

}
