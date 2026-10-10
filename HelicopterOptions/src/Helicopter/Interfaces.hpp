#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Log {

#if defined(_DEBUG)
    using Name = const char*;

    void Open(void* module);
    void Close();
    void CheckRotate();

    void Info(const char* format, ...);
    void Warn(const char* format, ...);
    void Error(const char* format, ...);
#else
    struct Name {
        constexpr Name(const char* = nullptr) {}
    };

    inline void Open(void*) {}
    inline void Close() {}
    inline void CheckRotate() {}

    template <typename... Args> void Info(const char*, const Args&...) {}
    template <typename... Args> void Warn(const char*, const Args&...) {}
    template <typename... Args> void Error(const char*, const Args&...) {}
#endif

}

namespace Memory {

    bool IsFinite(float value);
    bool CheckBytes(uintptr_t va, const uint8_t* expected, size_t length);
    bool Read(uintptr_t va, void* out, size_t length);
    bool WriteData(uintptr_t va, const void* bytes, size_t length);
    bool WriteCode(uintptr_t va, const void* bytes, size_t length);
    void DescribeBytes(uintptr_t va, size_t length, char* out, size_t outLength);

}

namespace Game {

    template <typename Return, typename... Args>
    Return ThisCall(uintptr_t function, Args... args) {
        return reinterpret_cast<Return (__thiscall*)(Args...)>(function)(args...);
    }

    template <typename Method>
    const void* MethodAddress(Method method) {
        static_assert(sizeof(method) == sizeof(void*), "only plain member functions have a single code address");
        const void* address = nullptr;
        std::memcpy(&address, &method, sizeof(address));
        return address;
    }

    template <typename T>
    T& Global(uintptr_t va) {
        return *reinterpret_cast<T*>(va);
    }

    inline uintptr_t VirtualFunction(const void* object, unsigned slot) {
        return (*static_cast<const uintptr_t* const*>(object))[slot / sizeof(uintptr_t)];
    }

}

namespace Detour {

    struct Registers {
        uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
        uint32_t eflags;
    };

    using Entry = void (__cdecl*)(Registers* registers);

    bool Install(Log::Name name, uintptr_t va, const uint8_t* prologue, size_t length, Entry entry);
    void RemoveAll();

}

namespace Patch {

    struct FloatOperand {
        uintptr_t va;
        uint8_t   opcode[2];
        uintptr_t constant;
    };

    struct FloatPush {
        uintptr_t va;
        float     value;
    };

    struct CallSite {
        uintptr_t va;
        uintptr_t target;
    };

    void Begin(Log::Name group);
    void RedirectFloat(Log::Name name, const FloatOperand& site, const float* value);
    void PushFloat(Log::Name name, const FloatPush& site, float value);
    void RedirectCall(Log::Name name, const CallSite& site, const void* replacement);
    void DataFloat(Log::Name name, uintptr_t va, float vanilla, float value);
    bool Commit();

    bool RewritePushedFloat(const FloatPush& site, float from, float to);

    int  SkippedGroups();
    int  FailedGroups();
    void RestoreAll();

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

}

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

enum SimableType : int;
enum ePlayerList {
    PLAYER_ALL    = 0,
    PLAYER_LOCAL  = 1,
    PLAYER_REMOTE = 2,
    PLAYER_MAX    = 3,
};

class IAttachableList;
class IPursuit;
class IRoadBlock;
class WRoadNav;
struct AITarget;
struct AvoidableList;
enum eLaneSelection : int;
class IPlayer;
class IRigidBody;
class IVehicle;
class WCollider;
class WWorldPos;
struct HSIMABLE__;
struct AISplinePath;

namespace Attrib {
    class Instance;

    namespace Gen {
        class aivehicle;
    }
}

namespace Sim {
    class IEntity;
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
};

class IPlayer : public UTL::COM::IUnknown {
  public:
    virtual ISimable* GetSimable() const = 0;

    static IPlayer* First(ePlayerList idx);
};

IRigidBody* GetLocalPlayerRigidBody();

class IVehicleAI : public UTL::COM::IUnknown {
  public:
    virtual ISimable* GetSimable() const = 0;
    virtual IVehicle* GetVehicle() const = 0;
    virtual const AISplinePath* GetSplinePath() = 0;
    virtual void SetReverseOverride(float time) = 0;
    virtual bool GetReverseOverride() = 0;
    virtual unsigned int GetDriveFlags() const = 0;
    virtual void ClearDriveFlags() = 0;
    virtual void DoReverse() = 0;
    virtual void DoSteering() = 0;
    virtual void DoGasBrake() = 0;
    virtual void DoDriving(unsigned int flags) = 0;
    virtual void DoNOS() = 0;
    virtual float GetDriveSpeed() = 0;
    virtual void SetDriveSpeed(float driveSpeed) = 0;
    virtual void SetDriveTarget(const UMath::Vector3& dest) = 0;
    virtual float GetLookAhead() = 0;
    virtual const UMath::Vector3& GetDriveTarget() = 0;
    virtual WRoadNav* GetDriveToNav() = 0;
    virtual bool GetDrivableToDriveToNav() const = 0;
    virtual void ResetDriveToNav(eLaneSelection lane_selection) = 0;
    virtual bool ResetVehicleToRoadNav(WRoadNav* other_nav) = 0;
    virtual bool ResetVehicleToRoadNav(short segInd, char laneInd, float timeStep) = 0;
    virtual bool ResetVehicleToRoadPos(const UMath::Vector3& position, const UMath::Vector3& forwardVector) = 0;
    virtual float GetPathDistanceRemaining() = 0;
    virtual AITarget* GetTarget() const = 0;
    virtual bool GetDrivableToTargetPos() const = 0;
    virtual const AvoidableList& GetAvoidableList() = 0;
    virtual void SetAvoidableRadius(float radius) = 0;
    virtual float GetTopSpeed() const = 0;
    virtual float GetAcceleration(float at) const = 0;
    virtual bool GetWorldAvoidanceInfo(float dT, UMath::Vector3& leftCollNormal, UMath::Vector3& rightCollNormal) const = 0;
    virtual WRoadNav* GetCollNav(const UMath::Vector3& forwardVector, float predictTime) = 0;
    virtual float GetLastSpawnTime() = 0;
    virtual void SetSpawned() = 0;
    virtual void UnSpawn() = 0;
    virtual bool CanRespawn(bool respawnAvailable) = 0;
    virtual const Attrib::Gen::aivehicle& GetAttributes() const = 0;
    virtual void EnableSimplePhysics() = 0;
    virtual void DisableSimplePhysics() = 0;
    virtual IPursuit* GetPursuit() = 0;
};

class IPursuit : public UTL::COM::IUnknown {
  public:
    virtual bool IsTarget(AITarget* aitarget) const = 0;
    virtual AITarget* GetTarget() const = 0;
    virtual int GetNumCops() const = 0;
    virtual int GetNumHeliSpawns() const = 0;
    virtual int GetNumCopsFullyEngaged() const = 0;
    virtual float GetPursuitDuration() const = 0;
    virtual float GetEvadeLevel() const = 0;
    virtual float GetCoolDownTimeRemaining() const = 0;
    virtual float GetCoolDownTimeRequired() const = 0;
    virtual bool IsPerpInSight() const = 0;
    virtual bool IsPursuitBailed() const = 0;
    virtual bool IsCollapseActive() const = 0;
    virtual bool AttemptingToReAquire() const = 0;
    virtual const UMath::Vector3& GetLastKnownLocation() const = 0;
};

class IAIHelicopter : public UTL::COM::IUnknown {
  public:
    virtual float GetDesiredHeightOverDest() const = 0;
    virtual void SetDesiredHeightOverDest(const float height) = 0;
    virtual void SetLookAtPosition(UMath::Vector3 la) = 0;
    virtual UMath::Vector3 GetLookAtPosition() const = 0;
    virtual void SetDestinationVelocity(const UMath::Vector3& v) = 0;
    virtual void SteerToNav(WRoadNav* road_nav, float height, float speed, bool bStopAtDest) = 0;
    virtual bool StartPathToPoint(UMath::Vector3& point) = 0;
    virtual bool StrafeToDestIsSet() const = 0;
    virtual void SetStrafeToDest(bool strafe) = 0;
    virtual bool FilterHeliAltitude(UMath::Vector3& point) = 0;
    virtual void RestrictPointToRoadNet(UMath::Vector3& seekPosition) = 0;
};

namespace UMath {

    inline Vector3 operator+(const Vector3& a, const Vector3& b) {
        return { a.x + b.x, a.y + b.y, a.z + b.z };
    }

    inline Vector3 operator-(const Vector3& a, const Vector3& b) {
        return { a.x - b.x, a.y - b.y, a.z - b.z };
    }

    inline Vector3 operator*(const Vector3& v, float scale) {
        return { v.x * scale, v.y * scale, v.z * scale };
    }

    float Distancexz(const Vector3& a, const Vector3& b);

}
