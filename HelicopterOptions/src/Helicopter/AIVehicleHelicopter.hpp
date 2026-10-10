#pragma once
#include "Interfaces.hpp"

class ISimpleChopper;

struct HeliSheetCoordinate {
    unsigned char mData[0x40];
};

class AIVehicleHelicopter {
  public:
    static void InstallPatches();
    static void Refresh();
    static bool HookOnDriving();
    static bool HookCanSeeTarget();

    IRigidBody* GetRigidBody() const {
        return mIOwner != nullptr ? mIOwner->GetRigidBody() : nullptr;
    }

    void UpdateHelicopterOptions();
    void BeginHelicopter(const UMath::Vector3& position);
    void AvoidCamera(UMath::Vector3& dest);
    void AvoidCameraHook(UMath::Vector3& dest);

    unsigned char       mBehavior[0x34];
    ISimable*           mIOwner;
    unsigned char       mAIVehicle[0x4C];
    float               mDriveSpeed;
    UMath::Vector3      mDest;
    unsigned char       mAIVehiclePursuit[0x710];
    unsigned char       mIAIHelicopter[0x8];
    UMath::Vector3      mDestinationVelocity;
    UMath::Vector3      mLookAtPosition;
    UMath::Vector3      mLastPlaceHeliSawPerp;
    float               mHeight;
    bool                mStrafeToDest;
    bool                mPerpHiddenFromMe;
    float               mHeliFuelTimeRemaining;
    float               mShadowScale;
    float               mDustStormIntensity;
    unsigned char       mHeliSheetCoordAlignment[0xC];
    HeliSheetCoordinate mHeliSheetCoord;
    HeliSheetCoordinate mSecondaryHeliSheetCoord;
    HeliSheetCoordinate mThirdHeliSheetCoord;
    ISimpleChopper*     mISimpleChopper;
};

bool HeliVehicleActive();
