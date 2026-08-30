#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cfloat>
#include "HeliState.h"
#include "../Core/Addresses.h"
#include "../Core/Memory.h"
#include "../Core/Log.h"

namespace HeliState {

    namespace {

        bool Finite(float v) { return v == v && v <= FLT_MAX && v >= -FLT_MAX; }
        bool SaneCoord(float v) { return Finite(v) && v > -1.0e6f && v < 1.0e6f; }
        bool SaneVel(float v)   { return Finite(v) && v > -1.0e5f && v < 1.0e5f; }

        typedef void* (__fastcall* ThiscallNoArg)(void* self, void* edx);
        typedef void* (__fastcall* ThiscallOneArg)(void* self, void* edx, void* a1);

        int SehCallVirt(void* obj, unsigned vtByteOffset, void** result) {
            __try {
                void** vtable = *reinterpret_cast<void***>(obj);
                if (!vtable) return 0;
                void* fn = vtable[vtByteOffset / sizeof(void*)];
                if (!fn) return 0;
                *result = reinterpret_cast<ThiscallNoArg>(fn)(obj, nullptr);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        int SehCallVirtOut(void* obj, unsigned vtByteOffset, void* outArg) {
            __try {
                void** vtable = *reinterpret_cast<void***>(obj);
                if (!vtable) return 0;
                void* fn = vtable[vtByteOffset / sizeof(void*)];
                if (!fn) return 0;
                reinterpret_cast<ThiscallOneArg>(fn)(obj, nullptr, outArg);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        int SehReadFloats(uintptr_t va, float* out, int count) {
            __try {
                memcpy(out, reinterpret_cast<const void*>(va), count * sizeof(float));
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        int SehWriteFloat(float* dst, float v) {
            __try { *dst = v; return 1; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        // ---- central rate-limited rejection reporter ------------------------
        void*         gRejLastPtr = nullptr;
        RejectReason  gRejLastReason = kOk;
        unsigned long gRejCount = 0;
        unsigned long gRejLastSummaryMs = 0;

        void ReportReject(void* ptr, RejectReason why) {
            const unsigned long now = GetTickCount();
            ++gRejCount;
            if (ptr != gRejLastPtr || why != gRejLastReason) {
                gRejLastPtr = ptr;
                gRejLastReason = why;
                Log::Info("[HeliState] snapshot rejected: ptr=%p reason=%s "
                          "(further identical rejections summarized).",
                          ptr, ReasonName(why));
            }
            if (now - gRejLastSummaryMs >= 30000) {
                if (gRejLastSummaryMs != 0 && gRejCount > 1) {
                    Log::Info("[HeliState] %lu snapshot rejection(s) in the last 30 s "
                              "(last: ptr=%p reason=%s).",
                              gRejCount, gRejLastPtr, ReasonName(gRejLastReason));
                }
                gRejLastSummaryMs = now;
                gRejCount = 0;
            }
        }

    } // namespace

    const char* ReasonName(RejectReason r) {
        switch (r) {
        case kOk:            return "ok";
        case kNoHeliGlobal:  return "no-helicopter-exists (gHeliVehicle==0)";
        case kBadOwner:      return "owner (+0x34) unreadable/null";
        case kBadRigidBody:  return "GetRigidBody chain failed";
        case kBadVectors:    return "position/velocity not finite/sane";
        case kBadDriveSpeed: return "drive speed not finite/sane";
        }
        return "?";
    }

    bool ValidateStructural(void* aiThis, void** ownerOut, void** rbOut,
                            RejectReason* why) {
        RejectReason r = kOk;
        void* owner = nullptr;
        void* rb = nullptr;

        void* globalHeli = nullptr;
        if (!Memory::ReadPtr(Addr::kGlobalHeliVehicle, &globalHeli) || !globalHeli) {
            r = kNoHeliGlobal;
        } else if (!aiThis
                   || !Memory::ReadPtr(reinterpret_cast<uintptr_t>(aiThis) + Addr::Heli::kOwner, &owner)
                   || !owner) {
            r = kBadOwner;
        } else if (!SehCallVirt(owner, Addr::OwnerVt::kGetRigidBody, &rb) || !rb) {
            r = kBadRigidBody;
        } else {
            void* posPtr = nullptr;
            void* velPtr = nullptr;
            float pos[3], vel[3], ds;
            if (!SehCallVirt(rb, Addr::RigidBodyVt::kGetPosition, &posPtr) || !posPtr
                || !SehCallVirt(rb, Addr::RigidBodyVt::kGetLinearVelocity, &velPtr) || !velPtr
                || !SehReadFloats(reinterpret_cast<uintptr_t>(posPtr), pos, 3)
                || !SehReadFloats(reinterpret_cast<uintptr_t>(velPtr), vel, 3)) {
                r = kBadRigidBody;
            } else if (!SaneCoord(pos[0]) || !SaneCoord(pos[1]) || !SaneCoord(pos[2])
                       || !SaneVel(vel[0]) || !SaneVel(vel[1]) || !SaneVel(vel[2])) {
                r = kBadVectors;
            } else if (!SehReadFloats(reinterpret_cast<uintptr_t>(aiThis) + Addr::Heli::kDriveSpeed, &ds, 1)
                       || !Finite(ds) || ds < -1.0e4f || ds > 1.0e4f) {
                r = kBadDriveSpeed;
            }
        }

        if (why) *why = r;
        if (r != kOk) return false;
        if (ownerOut) *ownerOut = owner;
        if (rbOut) *rbOut = rb;
        return true;
    }

    bool Capture(void* aiThis, Snapshot* out) {
        if (!out) return false;
        RejectReason why = kOk;
        void* owner = nullptr;
        void* rb = nullptr;
        if (!ValidateStructural(aiThis, &owner, &rb, &why)) {
            if (why != kNoHeliGlobal) ReportReject(aiThis, why);
            return false;
        }

        std::memset(out, 0, sizeof(*out));
        out->heli = aiThis;
        out->owner = owner;
        out->rigidBody = rb;

        const uintptr_t base = reinterpret_cast<uintptr_t>(aiThis);
        if (!SehReadFloats(base + Addr::Heli::kDriveSpeed, &out->driveSpeed, 1)
            || !SehReadFloats(base + Addr::Heli::kDest, out->dest, 3)
            || !SehReadFloats(base + Addr::Heli::kLookAtPosition, out->lookAt, 3)
            || !SehReadFloats(base + Addr::Heli::kFuelTimeRemaining, &out->fuel, 1)) {
            ReportReject(aiThis, kBadVectors);
            return false;
        }

        void* posPtr = nullptr;
        void* velPtr = nullptr;
        if (!SehCallVirt(rb, Addr::RigidBodyVt::kGetPosition, &posPtr) || !posPtr
            || !SehCallVirt(rb, Addr::RigidBodyVt::kGetLinearVelocity, &velPtr) || !velPtr
            || !SehReadFloats(reinterpret_cast<uintptr_t>(posPtr), out->pos, 3)
            || !SehReadFloats(reinterpret_cast<uintptr_t>(velPtr), out->vel, 3)) {
            ReportReject(aiThis, kBadRigidBody);
            return false;
        }
        out->velPtr = reinterpret_cast<float*>(velPtr);

        uint8_t skid = 0;
        Memory::ReadU8(Addr::kBIgnoreHeliSheet, &skid);
        out->skidActive = (skid != 0);

        // Orientation vectors (Experimental slots; SEH + sanity checked).
        out->fwdRightValid = false;
        float f[4] = {}, r2[4] = {};
        if (SehCallVirtOut(rb, Addr::RigidBodyVt::kGetForwardVector, f)
            && SehCallVirtOut(rb, Addr::RigidBodyVt::kGetRightVector, r2)) {
            const float fl = f[0]*f[0] + f[1]*f[1] + f[2]*f[2];
            const float rl = r2[0]*r2[0] + r2[1]*r2[1] + r2[2]*r2[2];
            const float dotFR = f[0]*r2[0] + f[1]*r2[1] + f[2]*r2[2];
            if (Finite(fl) && Finite(rl) && Finite(dotFR)
                && fl > 0.81f && fl < 1.21f
                && rl > 0.81f && rl < 1.21f
                && dotFR > -0.2f && dotFR < 0.2f) {
                std::memcpy(out->fwd, f, sizeof(out->fwd));
                std::memcpy(out->right, r2, sizeof(out->right));
                out->fwdRightValid = true;
            }
        }
        return true;
    }

    bool WriteHorizontalVelocity(const Snapshot& snap, float vx, float vz) {
        if (!snap.velPtr) return false;
        if (!Finite(vx) || !Finite(vz)) return false;
        if (!SehWriteFloat(&snap.velPtr[0], vx)) return false;
        if (!SehWriteFloat(&snap.velPtr[2], vz)) return false;
        return true;
    }

    bool WriteDriveSpeed(void* aiThis, float value) {
        if (!aiThis || !Finite(value)) return false;
        void* globalHeli = nullptr;
        if (!Memory::ReadPtr(Addr::kGlobalHeliVehicle, &globalHeli) || !globalHeli)
            return false;
        float* p = reinterpret_cast<float*>(
            reinterpret_cast<uintptr_t>(aiThis) + Addr::Heli::kDriveSpeed);
        return SehWriteFloat(p, value) != 0;
    }

    bool WriteSheetIgnore(bool ignore) {
        uint8_t v = ignore ? 1 : 0;
        return Memory::WriteBytes(Addr::kBIgnoreHeliSheet, &v, 1);
    }

    bool ReadPlayerKinematics(float pos[3], float vel[3]) {
        // IPlayer::First(PLAYER_LOCAL): count != 0, then *head.
        int count = 0;
        if (!SehReadFloats(Addr::kPlayerLocalListCount,
                           reinterpret_cast<float*>(&count), 1) || count == 0)
            return false;
        void* head = nullptr;
        if (!Memory::ReadPtr(Addr::kPlayerLocalListHead, &head) || !head) return false;
        void* player = nullptr;
        if (!Memory::ReadPtr(reinterpret_cast<uintptr_t>(head), &player) || !player) return false;

        void* simable = nullptr;
        if (!SehCallVirt(player, Addr::PlayerVt::kGetSimable, &simable) || !simable) return false;
        void* rb = nullptr;
        if (!SehCallVirt(simable, Addr::SimableVt::kGetRigidBody, &rb) || !rb) return false;

        void* posPtr = nullptr;
        void* velPtr = nullptr;
        if (!SehCallVirt(rb, Addr::RigidBodyVt::kGetPosition, &posPtr) || !posPtr
            || !SehCallVirt(rb, Addr::RigidBodyVt::kGetLinearVelocity, &velPtr) || !velPtr)
            return false;
        float p[3], v[3];
        if (!SehReadFloats(reinterpret_cast<uintptr_t>(posPtr), p, 3)
            || !SehReadFloats(reinterpret_cast<uintptr_t>(velPtr), v, 3))
            return false;
        for (int i = 0; i < 3; ++i) {
            if (!SaneCoord(p[i]) || !SaneVel(v[i])) return false;
        }
        std::memcpy(pos, p, sizeof(p));
        std::memcpy(vel, v, sizeof(v));
        return true;
    }

} // namespace HeliState
