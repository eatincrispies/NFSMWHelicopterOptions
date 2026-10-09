#pragma once
#include <cstddef>

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
class IPlayer;
class IRigidBody;
class IVehicle;
class WCollider;
class WWorldPos;
struct HSIMABLE__;
struct AISplinePath;

namespace Attrib {
    class Instance;
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
