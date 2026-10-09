#pragma once
#include "Interfaces.hpp"

class IAIHelicopter;
class IPursuitAI;

class AIActionHeliPursuit {
  public:
    enum kPursuitMode {
        kStraight_Line     = 0,
        kSearch_Pattern    = 1,
        kSkid_Hit_Approach = 2,
        kSkid_Hit_Strike   = 3,
    };

    static void InstallPatches();
    static void Refresh();
    static bool HookConstructor();
    static AIActionHeliPursuit* Find(const IRigidBody* heliRigidBody);

    bool IsSkidHitting() const {
        return mPursuitMode == kSkid_Hit_Approach || mPursuitMode == kSkid_Hit_Strike;
    }

    void SkidHitPursuit();
    void CrushPursuit();
    void StartCrush();

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
