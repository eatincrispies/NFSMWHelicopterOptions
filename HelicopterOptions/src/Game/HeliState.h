// HeliState.h - validated access to the live helicopter objects.
//
// Validation is STRUCTURAL, based on the verified ownership chain:
//   aiThis (+0x34) -> owner interface -> vtable+0x54 GetRigidBody()
//          -> vtable+0x20/+0x24 position / linear velocity
// A pointer is accepted only if the game's gHeliVehicle global is non-null,
// the whole chain resolves under SEH, and every value read is finite and
// sane. The owner pointer is the STABLE per-helicopter identity.
#pragma once
#include <cstdint>

namespace HeliState {

    enum RejectReason {
        kOk = 0,
        kNoHeliGlobal,
        kBadOwner,
        kBadRigidBody,
        kBadVectors,
        kBadDriveSpeed
    };
    const char* ReasonName(RejectReason r);

    struct Snapshot {
        void* heli;
        void* owner;
        void* rigidBody;
        float driveSpeed;
        float pos[3];
        float vel[3];
        float dest[3];
        float lookAt[3];
        float fuel;
        bool  skidActive;
        float* velPtr;
        // Orientation (IRigidBody vt+0x34 / vt+0x38, slots seen in the
        // decompiled pursuit actions). fwdRightValid=false when the calls
        // fail or return non-finite/non-unit vectors.
        float fwd[3];
        float right[3];
        bool  fwdRightValid;
    };

    bool ValidateStructural(void* aiThis, void** ownerOut, void** rbOut,
                            RejectReason* why);

    // Full snapshot. Failures are logged centrally (first occurrence per
    // pointer, reason changes, 30-second summaries) - callers must NOT add
    // their own reject logging.
    bool Capture(void* aiThis, Snapshot* out);

    bool WriteHorizontalVelocity(const Snapshot& snap, float vx, float vz);
    bool WriteDriveSpeed(void* aiThis, float value);
    bool WriteSheetIgnore(bool ignore);

    // REAL local-player kinematics via the verified IPlayer chain
    // (IPlayer::First(PLAYER_LOCAL) statics -> GetSimable vt+0x04 ->
    // GetRigidBody vt+0x54 -> position/velocity). The chain appears verbatim
    // in StartPathToPlayerCar (0x00427770) and its globals are byte-confirmed
    // in the executable. Returns false (no logging) when unavailable.
    bool ReadPlayerKinematics(float pos[3], float vel[3]);

} // namespace HeliState
