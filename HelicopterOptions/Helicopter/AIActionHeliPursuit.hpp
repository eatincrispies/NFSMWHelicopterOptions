#pragma once
#include "../../dllmain.hpp"

#define IPURSUIT_IVEHICLE_LIST           0x014
#define IPURSUIT_IS_PLAYER_PURSUIT       0x08C
#define IPURSUIT_SKID_HIT_ENABLED        0x11C

class IAIHelicopter;
class IPursuitAI;
class IPursuit;
class IVehicle;
class WRoadNav;
struct AISplinePath;
struct AITarget;
struct AvoidableList;

typedef int eLaneSelection;

namespace Attrib {
    namespace Gen {
        class aivehicle;
    }
}

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

    bool IsPlayerPursuit() const {
        return ((BoolQuery)VirtualFunction(this, IPURSUIT_IS_PLAYER_PURSUIT))(this);
    }

    bool SkidHitEnabled() const {
        return ((BoolQuery)VirtualFunction(this, IPURSUIT_SKID_HIT_ENABLED))(this);
    }
};

class AIActionHeliPursuit {
  public:
    enum kPursuitMode {
        kStraight_Line     = 0,
        kSearch_Pattern    = 1,
        kSkid_Hit_Approach = 2,
        kSkid_Hit_Strike   = 3
    };

    struct Settings {
        Settings();

        float LeadBase;
        float LeadMax;
        float AttackDuration;
        float ClosingSpeedNear;
        float ClosingSpeedMid;
        float ClosingSpeedFar;
        float SearchSpeedMin;
        float SearchSpeedMax;
        float SearchRadiusFirst;
        float SearchRadiusLargest;
    };

    static Settings sSettings;

    static void InstallPatches();
    static bool HookConstructor();
    static void Refresh();
    static void __cdecl ConstructorEntry(Detour::Registers* registers);
    static AIActionHeliPursuit* Find(const IRigidBody* heliRigidBody);

    bool IsSkidHitting() const {
        return mPursuitMode == kSkid_Hit_Approach || mPursuitMode == kSkid_Hit_Strike;
    }

    bool IsStriking() const {
        return mPursuitMode == kSkid_Hit_Strike;
    }

    void StraightLinePursuit();
    void ChasePerp();
    bool IsPerpUnderCover() const;
    bool IsPerpReversing() const;
    void LeadReversing(const UMath::Vector3& perpLinVel, float speed);
    void HoldFormationSpot(int place, const UMath::Vector3& perpLinVel);
    void ChaseToward(const UMath::Vector3& seek, const UMath::Vector3& perpLinVel);

    void SkidHitPursuit();
    void StrikePerp();
    bool ShouldAttackParkedPerp() const;
    void AttackParkedPerp();
    bool KeepsUpWhileReversing() const;
    bool ShouldStrikeNow() const;
    void BeginStrike();
    bool CanKeepDiving() const;
    void Dive();
    void BounceOff(const UMath::Vector3& outward);
    void HitCar(const UMath::Vector3& outward);

    void StartSearch();
    void SetNextSearchPoint();
    void SearchForPerp();
    void HoverAndLook(const UMath::Vector3& myPosition);
    UMath::Vector3 SearchLookAt(const UMath::Vector3& myPosition) const;

    unsigned char  mAIAction[0x4C];
    IVehicleAI*    mIVehicleAI;
    IVehicle*      mIVehicle;
    IRigidBody*    mIRigidBody;
    IAIHelicopter* mIAIHelicopter;
    IPursuitAI*    mIPursuitAI;
    float          mPursuitTime;
    float          mSkidKnockTimer;
    float          mPathTime;
    bool           mBuildingPath;
    float          mSearchPatternAngle;
    UMath::Vector3 mSearchDestPoint;
    IRigidBody*    mPlayerRigidBody;
    UMath::Vector3 mPlayerPosition;
    UMath::Vector3 mSkidHitOffset;
    int            mCollisionAbort;
    float          mPlayerSpeed;
    kPursuitMode   mPursuitMode;
};
