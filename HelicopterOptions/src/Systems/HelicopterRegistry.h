// HelicopterRegistry.h - per-instance helicopter lifecycle tracking.
//
// One physical helicopter = one record. Constructor records are provisional
// and MERGE into the AI mapping (owner match / learned delta / single
// candidate); merges never bump the alive count. Also carries last-known
// kinematics per instance for the [ChopperCollision] proximity telemetry
// (meaningful when spawner research creates 2+ helicopters).
#pragma once
#include <cstdint>

namespace Systems { namespace Registry {

    struct Record {
        void*         ctorPtr;
        void*         aiThis;
        void*         owner;
        void*         rigidBody;
        uint32_t      id;
        unsigned long ctorTickMs;
        unsigned long lastSeenMs;
        bool          alive;
        bool          mapped;
        // kinematics for proximity telemetry
        float         pos[3];
        float         vel[3];
        unsigned long kinMs;
    };

    void __cdecl OnCtor(void* ctorThis);
    void NotifyDriving(void* aiThis, void* owner, void* rigidBody);
    void UpdateKinematics(void* owner, const float pos[3], const float vel[3]);
    void Tick();          // gHeliVehicle watch, aging, proximity telemetry

    int  AliveCount();
    bool Enabled();

} } // namespace Systems::Registry
