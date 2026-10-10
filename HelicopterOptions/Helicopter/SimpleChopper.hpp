#pragma once
#include <cstdint>
#include "../../dllmain.hpp"

class ISimpleChopper;

namespace Attrib {

    class Collection;

    class Instance {
      public:
        UTL::COM::IUnknown* mOwner;
        const Collection*   mCollection;
        void*               mLayoutPtr;
        uint32_t            mMsgPort;
        uint16_t            mFlags;
        uint16_t            mLocks;
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
                return static_cast<_LayoutStruct*>(mLayoutPtr);
            }
        };

    }

}

class SimpleChopper {
  public:
    struct Settings {
        float TurnClamp         = 1.3f;
        float TurnResponseScale = -8.0f;
        float MaxChopperAccel   = 80.0f;
        float MinChopperAccel   = 30.0f;
        float SpeedCap          = 100.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:Turning",      "TurnClamp",         &sSettings.TurnClamp,         nullptr, 0.4f,   5.0f,   true },
        { "Helicopter:Turning",      "TurnResponseScale", &sSettings.TurnResponseScale, nullptr, -30.0f, -0.5f,  true },
        { "Helicopter:Acceleration", "MaxChopperAccel",   &sSettings.MaxChopperAccel,   nullptr, 40.0f,  250.0f, true },
        { "Helicopter:Acceleration", "MinChopperAccel",   &sSettings.MinChopperAccel,   nullptr, 0.0f,   160.0f, true },
        { "Helicopter:SpeedCap",     "SpeedCap",          &sSettings.SpeedCap,          nullptr, 30.0f,  500.0f, true },
    };

    static constexpr uintptr_t kMax_Chopper_Accel = 0x008F8DCCu;
    static constexpr uintptr_t kMin_Chopper_Accel = 0x008F8DD0u;

    static constexpr Patch::FloatOperand kTotalTorqueYScale     = { 0x006A28A8u, { 0xD8, 0x0D }, 0x008AB8ECu };
    static constexpr Patch::FloatOperand kAngVelYClampTest      = { 0x006A28BAu, { 0xD8, 0x1D }, 0x008AAE5Cu };
    static constexpr Patch::FloatOperand kAngVelYClampHigh      = { 0x006A2994u, { 0xD9, 0x05 }, 0x008AAE5Cu };
    static constexpr Patch::FloatOperand kAngVelYClampLowTest   = { 0x006A28CFu, { 0xD9, 0x05 }, 0x008AB8E8u };
    static constexpr Patch::FloatOperand kAngVelYClampLow       = { 0x006A28E2u, { 0xD9, 0x05 }, 0x008AB8E8u };
    static constexpr Patch::FloatOperand kSimulateDeltaTimeGate = { 0x006A2762u, { 0xD8, 0x1D }, 0x00890EC4u };

    static constexpr Patch::FloatOperand kLastAngVelocityScale[3] = {
        { 0x006A28EFu, { 0xD8, 0x0D }, 0x008A0718u },
        { 0x006A28FCu, { 0xD8, 0x0D }, 0x008A0718u },
        { 0x006A2908u, { 0xD8, 0x0D }, 0x008A0718u },
    };
    static constexpr Patch::FloatOperand kLastAngVelocityBlend[3] = {
        { 0x006A291Eu, { 0xD8, 0x0D }, 0x00890F14u },
        { 0x006A292Bu, { 0xD8, 0x0D }, 0x00890F14u },
        { 0x006A293Cu, { 0xD8, 0x0D }, 0x00890F14u },
    };
    static constexpr Patch::FloatOperand kLastAccelVectorScale[3] = {
        { 0x006A204Fu, { 0xD8, 0x0D }, 0x00890E98u },
        { 0x006A205Bu, { 0xD8, 0x0D }, 0x00890E98u },
        { 0x006A2067u, { 0xD8, 0x0D }, 0x00890E98u },
    };
    static constexpr Patch::FloatOperand kLastAccelVectorBlend[3] = {
        { 0x006A2075u, { 0xD8, 0x0D }, 0x00895074u },
        { 0x006A2083u, { 0xD8, 0x0D }, 0x00895074u },
        { 0x006A2092u, { 0xD8, 0x0D }, 0x00895074u },
    };

    static constexpr float kGameLastAngVelocityScale = 7.0f;
    static constexpr float kGameLastAngVelocityBlend = 0.125f;
    static constexpr float kGameLastAccelVectorScale = 4.0f;
    static constexpr float kGameLastAccelVectorBlend = 0.2f;
    static constexpr float kGameMax_Chopper_Accel    = 80.0f;
    static constexpr float kGameMin_Chopper_Accel    = 30.0f;
    static constexpr float kLowestDeltaTimeGate      = 0.0001f;

    struct LiveValues {
        float totalTorqueYScale;
        float angVelYClamp;
        float angVelYClampLow;
        float lastAngVelocityScale;
        float lastAngVelocityBlend;
        float lastAccelVectorScale;
        float lastAccelVectorBlend;
        float simulateDeltaTimeGate;
    };

    static inline LiveValues sLive = {};
    static inline bool       sAccelPatched = false;
    static inline float      sMax_Chopper_Accel = 0.0f;
    static inline float      sMin_Chopper_Accel = 0.0f;

    static void InstallPatches() {
        Refresh();
        sLive.lastAngVelocityScale  = kGameLastAngVelocityScale;
        sLive.lastAngVelocityBlend  = kGameLastAngVelocityBlend;
        sLive.lastAccelVectorScale  = kGameLastAccelVectorScale;
        sLive.lastAccelVectorBlend  = kGameLastAccelVectorBlend;
        sLive.simulateDeltaTimeGate = kLowestDeltaTimeGate;

        Patch::Begin("SimpleChopper::OnTaskSimulate");
        Patch::RedirectFloat("angVel.y = -totalTorque.y * 8.0", kTotalTorqueYScale, &sLive.totalTorqueYScale);
        Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) test", kAngVelYClampTest, &sLive.angVelYClamp);
        Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) high", kAngVelYClampHigh, &sLive.angVelYClamp);
        Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) low test", kAngVelYClampLowTest, &sLive.angVelYClampLow);
        Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) low", kAngVelYClampLow, &sLive.angVelYClampLow);
        for (const Patch::FloatOperand& site : kLastAngVelocityScale)
            Patch::RedirectFloat("UMath::Scale(mLastAngVelocity, 7.0)", site, &sLive.lastAngVelocityScale);
        for (const Patch::FloatOperand& site : kLastAngVelocityBlend)
            Patch::RedirectFloat("UMath::AddScale(angVel, mLastAngVelocity, 0.125)", site, &sLive.lastAngVelocityBlend);
        Patch::RedirectFloat("dT > 0.005", kSimulateDeltaTimeGate, &sLive.simulateDeltaTimeGate);
        Patch::Commit();

        Patch::Begin("SimpleChopper::SetTorqueToMatchPitchAndRoll");
        for (const Patch::FloatOperand& site : kLastAccelVectorScale)
            Patch::RedirectFloat("UMath::Scale(mLastAccelVector, 4.0)", site, &sLive.lastAccelVectorScale);
        for (const Patch::FloatOperand& site : kLastAccelVectorBlend)
            Patch::RedirectFloat("UMath::AddScale(localXZAccel, mLastAccelVector, 0.2)", site, &sLive.lastAccelVectorBlend);
        Patch::Commit();

        Patch::Begin("Max_Chopper_Accel and Min_Chopper_Accel");
        Patch::DataFloat("Max_Chopper_Accel", kMax_Chopper_Accel, kGameMax_Chopper_Accel, sSettings.MaxChopperAccel);
        Patch::DataFloat("Min_Chopper_Accel", kMin_Chopper_Accel, kGameMin_Chopper_Accel, sSettings.MinChopperAccel);
        sAccelPatched = Patch::Commit();
        sMax_Chopper_Accel = sSettings.MaxChopperAccel;
        sMin_Chopper_Accel = sSettings.MinChopperAccel;
    }

    static void Refresh();
    static void ScaleMotionFilters(float frames);
    static void ApplySpeedCap(ISimpleChopper* ichopper);
    static void RestoreSpeedCap();

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
