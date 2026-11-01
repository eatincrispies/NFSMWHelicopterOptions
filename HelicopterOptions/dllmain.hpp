#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <string.h>

#define SIM_GET_TIME                     0x006E8DE0

#define IRIGIDBODY_GET_DIMENSION         0x048
#define IRIGIDBODY_SET_LINEAR_VELOCITY   0x060
#define IRIGIDBODY_SET_ANGULAR_VELOCITY  0x064

typedef void  (__thiscall* VoidMethod)(void* object);
typedef void  (__thiscall* IntMethod)(void* object, int value);
typedef void  (__thiscall* PointerMethod)(void* object, const void* value);
typedef bool  (__thiscall* BoolQuery)(const void* object);
typedef float (__thiscall* FloatQuery)(const void* object);
typedef void* (__thiscall* PointerQuery)(const void* object);
typedef float (__cdecl* FloatFunction)(void);

namespace Memory {

    bool IsFinite(float value);
    bool Matches(DWORD address, const BYTE* expected, size_t length);
    bool Read(DWORD address, void* out, size_t length);
    bool WriteData(DWORD address, const void* bytes, size_t length);
    bool WriteCode(DWORD address, const void* bytes, size_t length);

}

float Seconds(int milliseconds);
float Metres(int millimetres);
float Fraction(int percent);
float Smaller(float a, float b);
float Larger(float a, float b);
float Clamp(float value, float low, float high);
int   ClampInt(int value, int low, int high);

inline DWORD VirtualFunction(const void* object, DWORD slot) {
    return (*(const DWORD* const*)object)[slot / sizeof(DWORD)];
}

template <typename Method>
const void* MethodAddress(Method method) {
    C_ASSERT(sizeof(Method) == sizeof(void*));
    const void* address = NULL;
    memcpy(&address, &method, sizeof(address));
    return address;
}

namespace Detour {

    struct Registers {
        DWORD edi, esi, ebp, esp, ebx, edx, ecx, eax;
        DWORD eflags;
    };

    typedef void (__cdecl* Entry)(Registers* registers);

    bool Install(DWORD address, const BYTE* prologue, size_t length, Entry entry);
    void RemoveAll();

}

namespace Patch {

    struct FloatOperand {
        DWORD address;
        BYTE  opcode[2];
        DWORD constant;
    };

    struct FloatPush {
        DWORD address;
        float value;
    };

    struct CallSite {
        DWORD address;
        DWORD target;
    };

    struct FloatStore {
        DWORD address;
        BYTE  opcode[3];
        float value;
    };

    struct ByteStore {
        DWORD address;
        BYTE  code[6];
        BYTE  value;
    };

    struct IndirectCall {
        DWORD address;
        BYTE  code[6];
    };

    struct JumpSite {
        DWORD address;
        BYTE  opcode[2];
        DWORD target;
    };

    void Begin();
    void RedirectFloat(const FloatOperand& site, const float* value);
    void PushFloat(const FloatPush& site, float value);
    void RedirectCall(const CallSite& site, const void* replacement);
    void RedirectIndirectCall(const IndirectCall& site, const void* replacement);
    void RedirectJump(const JumpSite& site, const void* replacement);
    void RedirectVirtual(DWORD slot, DWORD function, const void* replacement);
    void DataFloat(DWORD address, float vanilla, float value);
    void StoreFloat(const FloatStore& site, float value);
    void StoreByte(const ByteStore& site, BYTE value);
    bool Commit();

    bool RewritePushedFloat(const FloatPush& site, float from, float to);
    bool RewriteStoredFloat(const FloatStore& site, float from, float to);
    bool RewriteStoredByte(const ByteStore& site, BYTE from, BYTE to);

    void RestoreAll();

}

namespace Ini {

    void Load(HMODULE module);
    bool ApplyHeat(int level, bool racing);

}

template <typename State, int Count>
class PerHelicopter {
  public:
    PerHelicopter() : mClock(0) {
        for (int i = 0; i < Count; ++i) Clear(mSlots[i]);
    }

    State& Use(const void* owner) {
        Slot* oldest = &mSlots[0];
        for (int i = 0; i < Count; ++i) {
            if (mSlots[i].owner == owner) return Touch(mSlots[i]);
            if (mSlots[i].used < oldest->used) oldest = &mSlots[i];
        }
        oldest->owner = owner;
        oldest->state = State();
        return Touch(*oldest);
    }

    void Forget(const void* owner) {
        for (int i = 0; i < Count; ++i)
            if (mSlots[i].owner == owner) Clear(mSlots[i]);
    }

  private:
    struct Slot {
        const void* owner;
        unsigned    used;
        State       state;
    };

    static void Clear(Slot& slot) {
        slot.owner = NULL;
        slot.used = 0;
        slot.state = State();
    }

    State& Touch(Slot& slot) {
        slot.used = ++mClock;
        return slot.state;
    }

    Slot     mSlots[Count];
    unsigned mClock;
};

namespace UTL {
namespace COM {

    class Object;

    class IUnknown {
      protected:
        virtual ~IUnknown() {}

      private:
        Object* _mCOMObject;
    };

}
}

namespace UMath {

    struct Vector3 {
        float x, y, z;
    };

    struct Vector4 {
        float x, y, z, w;
    };

    struct Matrix4 {
        Vector4 v0, v1, v2, v3;
    };

    inline Vector3 Vector(float x, float y, float z) {
        Vector3 v;
        v.x = x;
        v.y = y;
        v.z = z;
        return v;
    }

    inline Vector3 operator+(const Vector3& a, const Vector3& b) {
        return Vector(a.x + b.x, a.y + b.y, a.z + b.z);
    }

    inline Vector3 operator-(const Vector3& a, const Vector3& b) {
        return Vector(a.x - b.x, a.y - b.y, a.z - b.z);
    }

    inline Vector3 operator*(const Vector3& v, float scale) {
        return Vector(v.x * scale, v.y * scale, v.z * scale);
    }

    inline float Dot(const Vector3& a, const Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline Vector3 Cross(const Vector3& a, const Vector3& b) {
        return Vector(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
    }

    inline bool IsFinite(const Vector3& v) {
        return Memory::IsFinite(v.x) && Memory::IsFinite(v.y) && Memory::IsFinite(v.z);
    }

    float Distancexz(const Vector3& a, const Vector3& b);

}

typedef int SimableType;

enum ePlayerList {
    PLAYER_ALL    = 0,
    PLAYER_LOCAL  = 1,
    PLAYER_REMOTE = 2,
    PLAYER_MAX    = 3
};

class IAttachableList;
class IPlayer;
class IRigidBody;
class WWorldPos;
struct HSIMABLE__;

namespace Attrib {
    class Instance;
}

namespace Sim {

    class IEntity;

    inline float GetTime() {
        return ((FloatFunction)SIM_GET_TIME)();
    }

}

class ISimable : public UTL::COM::IUnknown {
  public:
    virtual SimableType GetSimableType() const = 0;
    virtual void Kill() = 0;
    virtual bool Attach(UTL::COM::IUnknown* object) = 0;
    virtual bool Detach(UTL::COM::IUnknown* object) = 0;
    virtual const IAttachableList* GetAttachments() const = 0;
    virtual void AttachEntity(Sim::IEntity* e) = 0;
    virtual void DetachEntity() = 0;
    virtual IPlayer* GetPlayer() const = 0;
    virtual bool IsPlayer() const = 0;
    virtual bool IsOwnedByPlayer() const = 0;
    virtual Sim::IEntity* GetEntity() const = 0;
    virtual void DebugObject() = 0;
    virtual HSIMABLE__* GetOwnerHandle() const = 0;
    virtual ISimable* GetOwner() const = 0;
    virtual bool IsOwnedBy(ISimable* queriedOwner) const = 0;
    virtual void SetOwnerObject(ISimable* pOwner) = 0;
    virtual const Attrib::Instance& GetAttributes() const = 0;
    virtual WWorldPos& GetWPos() = 0;
    virtual const WWorldPos& GetWPos() const = 0;
    virtual IRigidBody* GetRigidBody() = 0;
    virtual const IRigidBody* GetRigidBody() const = 0;
};

class IRigidBody : public UTL::COM::IUnknown {
  public:
    virtual ISimable* GetOwner() const = 0;
    virtual bool IsSimple() const = 0;
    virtual int GetIndex() const = 0;
    virtual SimableType GetSimableType() const = 0;
    virtual float GetRadius() const = 0;
    virtual float GetMass() const = 0;
    virtual float GetOOMass() const = 0;
    virtual const UMath::Vector3& GetPosition() const = 0;
    virtual const UMath::Vector3& GetLinearVelocity() const = 0;
    virtual const UMath::Vector3& GetAngularVelocity() const = 0;
    virtual float GetSpeed() const = 0;
    virtual float GetSpeedXZ() const = 0;
    virtual void GetForwardVector(UMath::Vector3& vec) const = 0;
    virtual void GetRightVector(UMath::Vector3& vec) const = 0;
    virtual void GetUpVector(UMath::Vector3& vec) const = 0;

    void GetDimension(UMath::Vector3& dim) const {
        ((PointerMethod)VirtualFunction(this, IRIGIDBODY_GET_DIMENSION))((void*)this, &dim);
    }

    void SetLinearVelocity(const UMath::Vector3& vel) {
        ((PointerMethod)VirtualFunction(this, IRIGIDBODY_SET_LINEAR_VELOCITY))(this, &vel);
    }

    void SetAngularVelocity(const UMath::Vector3& vel) {
        ((PointerMethod)VirtualFunction(this, IRIGIDBODY_SET_ANGULAR_VELOCITY))(this, &vel);
    }
};

class IPlayer : public UTL::COM::IUnknown {
  public:
    virtual ISimable* GetSimable() const = 0;

    static IPlayer* First(ePlayerList idx);
};

IRigidBody* GetLocalPlayerRigidBody();

class AIPerpVehicle {
  public:
    static bool HookSetHeat();
    static void __cdecl SetHeatEntry(Detour::Registers* registers);
    static void UpdateHeat();

    bool IsLocalPlayer() const;

    unsigned char mBehavior[0x34];
    ISimable*     mIOwner;
    unsigned char mAIVehiclePid[0x720];
    unsigned char mIPerpetrator[0x1C];
    float         mHeat;
    int           mCostToState;
    int           mPendingRepPointsNormal;
    int           mPendingRepPointsFromCopDestruction;
    bool          mHiddenFromCars;
    bool          mHiddenFromHelicopters;
    bool          mWasInRaceEventLastHeatUpdate;
};
