#pragma once
#include "../../dllmain.hpp"

class ISimpleChopper;

namespace Attrib {

    class Collection;

    class Instance {
      public:
        UTL::COM::IUnknown* mOwner;
        const Collection*   mCollection;
        void*               mLayoutPtr;
        DWORD               mMsgPort;
        WORD                mFlags;
        WORD                mLocks;
    };

    namespace Gen {

        class chopperspecs : public Instance {
          public:
            struct _LayoutStruct {
                UMath::Vector4 AIR_RESISTANCE_SCALE;
                float          YAW_STRENGTH_FRONT;
                float          YAW_DAMP;
                float          YAW_LIMIT_FRONT;
                float          PITCH_ALIGN_SCALE;
                float          AIR_RESISTANCE;
                float          PITCH_STOP_SPEED;
                float          YAW_BOOST_LIMIT;
                float          ROLL_SPEED_MIN;
                float          ROLL_SLOW_DOWN_RATE;
                float          ROLL_START_SPEED;
                float          PITCH_ANG;
                float          YAW_STRENGTH_REAR;
                float          ROLL_ANG;
                float          MAX_SPEED_MPS;
                float          STRAFE_SCALEY;
                float          STRAFE_SCALEX;
                float          YAW_LIMIT_REAR;
                float          TURN_BOOST_SPEED;
                float          PITCH_SLOW_DOWN_RATE;
                float          DRIVE_SPEED;
                float          ROLL_ALIGN_SCALE;
                float          SAME_ALIGN_SCALE;
                bool           SCALE_STEERING;
            };

            _LayoutStruct* GetLayout() const {
                return (_LayoutStruct*)mLayoutPtr;
            }
        };

    }

}

class SimpleChopper {
  public:
    struct Settings {
        Settings();

        float TurnClamp;
        float TurnResponseScale;
        float MaxChopperAccel;
        float MinChopperAccel;
        float SpeedCap;
    };

    static Settings sSettings;

    static void InstallPatches();
    static void Refresh();
    static void ApplySpeedCap(ISimpleChopper* ichopper);
    static void RestoreSpeedCap();
    static void RestoreWrecks();
    static bool IsWreck(ISimpleChopper* ichopper);
    static float ClimbLimit(const UMath::Vector3& position, float flatSpeed);

    void OnTaskSimulate(float dT);
    void LimitClimb();
    void KeepLevelNearGround();
    bool IsDestroyed() const;
    void Crash(float dT);
    void Recover();

    unsigned char             mVehicleBehavior[0x4C];
    unsigned char             mISimpleChopper[0x8];
    UMath::Vector3            mLastBodyOffset;
    UMath::Vector3            mLastAngVelocity;
    UMath::Vector3            mLastAccelVector;
    UMath::Vector3            mDesiredVelocity;
    UMath::Vector3            mPreviousVelocity;
    UMath::Vector3            mDesiredFacingVector;
    Attrib::Gen::chopperspecs mChopperSpecs;
    Attrib::Instance          mVehicleSpecs;
    bool                      mMaxDecelFlag;
    IRigidBody*               mIrigidBody;
    void*                     mIrbComplex;
    void*                     mIdamage;
};
