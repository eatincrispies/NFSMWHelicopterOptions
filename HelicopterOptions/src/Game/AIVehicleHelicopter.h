#pragma once

namespace AIVehicleHelicopter {

    struct Snapshot {
        void* heli;
        void* owner;
        void* rigidBody;
        void* chopper;
        float position[3];
        float fuel;
        int   mode;
    };

    bool  Capture(void* heli, Snapshot* out);
    bool  WriteFuelTime(const Snapshot& snapshot, float seconds);
    bool  HookOnDriving();
    bool  HookLineOfSight();
    float StepSeconds();

}
