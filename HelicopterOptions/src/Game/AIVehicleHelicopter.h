#pragma once

namespace AIVehicleHelicopter {

    struct Snapshot {
        void*  heli;
        void*  owner;
        void*  rigidBody;
        float* velocityPointer;
        float  position[3];
        float  velocity[3];
        float  driveSpeed;
        float  fuel;
        int    mode;
    };

    bool Capture(void* heli, Snapshot* out);

    bool WriteDriveSpeed(const Snapshot& snapshot, float speed);
    bool WriteVerticalVelocity(const Snapshot& snapshot, float y);
    bool WriteFuelTime(const Snapshot& snapshot, float seconds);

    bool HookOnDriving();

}
