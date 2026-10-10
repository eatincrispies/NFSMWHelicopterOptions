#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "SimpleChopper.hpp"

namespace {

    constexpr uintptr_t kVTableForISimpleChopper = 0x008AB86Cu;
    constexpr int       kTrackedSpecs            = 4;
    constexpr float     kSafeTurningAuthority    = 1.75f;

    static_assert(offsetof(SimpleChopper, mISimpleChopper) == 0x4C, "SimpleChopper's ISimpleChopper");
    static_assert(offsetof(SimpleChopper, mLastBodyOffset) == 0x54, "SimpleChopper::mLastBodyOffset");
    static_assert(offsetof(SimpleChopper, mLastAngVelocity) == 0x60, "SimpleChopper::mLastAngVelocity");
    static_assert(offsetof(SimpleChopper, mLastAccelVector) == 0x6C, "SimpleChopper::mLastAccelVector");
    static_assert(offsetof(SimpleChopper, mDesiredVelocity) == 0x78, "SimpleChopper::mDesiredVelocity");
    static_assert(offsetof(SimpleChopper, mPreviousVelocity) == 0x84, "SimpleChopper::mPreviousVelocity");
    static_assert(offsetof(SimpleChopper, mDesiredFacingVector) == 0x90, "SimpleChopper::mDesiredFacingVector");
    static_assert(offsetof(SimpleChopper, mChopperSpecs) == 0x9C, "SimpleChopper::mChopperSpecs");
    static_assert(offsetof(SimpleChopper, mMaxDecelFlag) == 0xC4, "SimpleChopper::mMaxDecelFlag");
    static_assert(offsetof(SimpleChopper, mIrigidBody) == 0xC8, "SimpleChopper::mIrigidBody");
    static_assert(offsetof(Attrib::Instance, mLayoutPtr) == 0x08, "Attrib::Instance::mLayoutPtr");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, YAW_DAMP) == 0x14, "chopperspecs YAW_DAMP");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, AIR_RESISTANCE) == 0x20, "chopperspecs AIR_RESISTANCE");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, PITCH_ANG) == 0x38, "chopperspecs PITCH_ANG");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, ROLL_ANG) == 0x40, "chopperspecs ROLL_ANG");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, MAX_SPEED_MPS) == 0x44, "chopperspecs MAX_SPEED_MPS");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, DRIVE_SPEED) == 0x5C, "chopperspecs DRIVE_SPEED");

    struct TrackedSpecs {
        Attrib::Gen::chopperspecs::_LayoutStruct* layout;
        float                                     gameMaxSpeed;
    };

    TrackedSpecs gSpecs[kTrackedSpecs] = {};
    int          gSpecsCount = 0;

    void ScaleFilterPair(float scale, float blend, float frames, float* outScale, float* outBlend) {
        *outScale = scale;
        *outBlend = blend;

        const float retained = scale * blend;
        const float steps = std::isfinite(frames) ? std::clamp(frames, 0.0f, 64.0f) : 1.0f;
        if (!(retained > 0.0f) || retained >= 1.0f || steps <= 0.0f || steps == 1.0f) return;

        const float retainedNow = std::pow(retained, steps);
        if (!std::isfinite(retainedNow) || retainedNow <= 0.0f || retainedNow >= 0.999999f) return;

        const float newBlend = 1.0f - retainedNow;
        *outBlend = newBlend;
        *outScale = retainedNow / newBlend;
    }

    SimpleChopper* FromISimpleChopper(ISimpleChopper* ichopper) {
        uintptr_t vtable = 0;
        const uintptr_t va = reinterpret_cast<uintptr_t>(ichopper);
        if (!ichopper || !Memory::Read(va, &vtable, sizeof(vtable)) || vtable != kVTableForISimpleChopper) return nullptr;
        return reinterpret_cast<SimpleChopper*>(va - offsetof(SimpleChopper, mISimpleChopper));
    }

    Attrib::Gen::chopperspecs::_LayoutStruct* ChopperSpecs(ISimpleChopper* ichopper) {
        SimpleChopper* chopper = FromISimpleChopper(ichopper);
        Attrib::Gen::chopperspecs::_LayoutStruct* layout = chopper ? chopper->mChopperSpecs.GetLayout() : nullptr;
        if (!layout) return nullptr;
        const float speed = layout->MAX_SPEED_MPS;
        return Memory::IsFinite(speed) && speed >= 1.0f && speed <= 10000.0f ? layout : nullptr;
    }

    const TrackedSpecs* Track(Attrib::Gen::chopperspecs::_LayoutStruct* layout) {
        for (int i = 0; i < gSpecsCount; ++i)
            if (gSpecs[i].layout == layout) return &gSpecs[i];
        if (gSpecsCount == kTrackedSpecs) return nullptr;
        gSpecs[gSpecsCount] = { layout, layout->MAX_SPEED_MPS };
        Log::Info("chopperspecs  _LayoutStruct=0x%p gSpecs[%d] MAX_SPEED_MPS[+0x44]=%g", layout, gSpecsCount, layout->MAX_SPEED_MPS);
        return &gSpecs[gSpecsCount++];
    }

}

void SimpleChopper::Refresh() {
    if (sSettings.MinChopperAccel > sSettings.MaxChopperAccel) {
        Log::Warn("[Helicopter:Acceleration] MinChopperAccel=%g > MaxChopperAccel=%g  -> %g", sSettings.MinChopperAccel, sSettings.MaxChopperAccel,
                  sSettings.MaxChopperAccel);
        sSettings.MinChopperAccel = sSettings.MaxChopperAccel;
    }

    const Settings gameValues{};
    const float authority = std::fabs(sSettings.TurnResponseScale / gameValues.TurnResponseScale) * (sSettings.TurnClamp / gameValues.TurnClamp);
    if (authority >= kSafeTurningAuthority)
        Log::Warn("[Helicopter:Turning] |%g / %g| x (%g / %g) = %.3f >= %.2f", sSettings.TurnResponseScale, gameValues.TurnResponseScale,
                  sSettings.TurnClamp, gameValues.TurnClamp, authority, kSafeTurningAuthority);

    sLive.totalTorqueYScale = sSettings.TurnResponseScale;
    sLive.angVelYClamp      = sSettings.TurnClamp;
    sLive.angVelYClampLow   = -sSettings.TurnClamp;
    Log::Info("SimpleChopper  sLive@0x%p totalTorqueYScale=%g angVelYClamp=%g angVelYClampLow=%g", &sLive, sLive.totalTorqueYScale, sLive.angVelYClamp,
              sLive.angVelYClampLow);

    if (sAccelPatched && (sSettings.MaxChopperAccel != sMax_Chopper_Accel || sSettings.MinChopperAccel != sMin_Chopper_Accel)) {
        Log::Info("Max_Chopper_Accel[0x%08X] %g -> %g  Min_Chopper_Accel[0x%08X] %g -> %g", kMax_Chopper_Accel, Game::Global<float>(kMax_Chopper_Accel),
                  sSettings.MaxChopperAccel, kMin_Chopper_Accel, Game::Global<float>(kMin_Chopper_Accel), sSettings.MinChopperAccel);
        Game::Global<float>(kMax_Chopper_Accel) = sSettings.MaxChopperAccel;
        Game::Global<float>(kMin_Chopper_Accel) = sSettings.MinChopperAccel;
        sMax_Chopper_Accel = sSettings.MaxChopperAccel;
        sMin_Chopper_Accel = sSettings.MinChopperAccel;
    }
}

void SimpleChopper::ScaleMotionFilters(float frames) {
    ScaleFilterPair(kGameLastAngVelocityScale, kGameLastAngVelocityBlend, frames, &sLive.lastAngVelocityScale, &sLive.lastAngVelocityBlend);
    ScaleFilterPair(kGameLastAccelVectorScale, kGameLastAccelVectorBlend, frames, &sLive.lastAccelVectorScale, &sLive.lastAccelVectorBlend);
}

void SimpleChopper::ApplySpeedCap(ISimpleChopper* ichopper) {
    Attrib::Gen::chopperspecs::_LayoutStruct* layout = ChopperSpecs(ichopper);
    if (!layout) return;
    const TrackedSpecs* tracked = Track(layout);
    if (!tracked || layout->MAX_SPEED_MPS == sSettings.SpeedCap || !Memory::IsFinite(sSettings.SpeedCap)) return;

    Log::Info("chopperspecs  _LayoutStruct=0x%p MAX_SPEED_MPS[+0x44] %g -> %g  game=%g ISimpleChopper=0x%p", layout, layout->MAX_SPEED_MPS,
              sSettings.SpeedCap, tracked->gameMaxSpeed, ichopper);
    layout->MAX_SPEED_MPS = sSettings.SpeedCap;
}

void SimpleChopper::RestoreSpeedCap() {
    for (int i = 0; i < gSpecsCount; ++i) {
        if (Memory::WriteData(reinterpret_cast<uintptr_t>(&gSpecs[i].layout->MAX_SPEED_MPS), &gSpecs[i].gameMaxSpeed, sizeof(float)))
            Log::Info("restore chopperspecs  _LayoutStruct=0x%p MAX_SPEED_MPS[+0x44] -> %g", gSpecs[i].layout, gSpecs[i].gameMaxSpeed);
        else
            Log::Warn("restore chopperspecs  _LayoutStruct=0x%p MAX_SPEED_MPS[+0x44] unwritable", gSpecs[i].layout);
    }
    gSpecsCount = 0;
}

#if defined(_DEBUG)
void SimpleChopper::LogLive(ISimpleChopper* ichopper) {
    const SimpleChopper* chopper = FromISimpleChopper(ichopper);
    if (chopper == nullptr) {
        uintptr_t vtable = 0;
        Memory::Read(reinterpret_cast<uintptr_t>(ichopper), &vtable, sizeof(vtable));
        Log::Warn("SimpleChopper  ISimpleChopper=0x%p vtbl=0x%08X, expected 0x%08X", ichopper, vtable, kVTableForISimpleChopper);
        return;
    }

    Log::Info("SimpleChopper 0x%p  ISimpleChopper[+0x4C]=0x%p vtbl=0x%08X mIrigidBody[+0xC8]=0x%p mMaxDecelFlag[+0xC4]=%d", chopper, ichopper,
              kVTableForISimpleChopper, chopper->mIrigidBody, chopper->mMaxDecelFlag);
    Log::Info("  mLastBodyOffset[+0x54]=(%.3f, %.3f, %.3f) mLastAngVelocity[+0x60]=(%.3f, %.3f, %.3f) mLastAccelVector[+0x6C]=(%.3f, %.3f, %.3f)",
              chopper->mLastBodyOffset.x, chopper->mLastBodyOffset.y, chopper->mLastBodyOffset.z, chopper->mLastAngVelocity.x, chopper->mLastAngVelocity.y,
              chopper->mLastAngVelocity.z, chopper->mLastAccelVector.x, chopper->mLastAccelVector.y, chopper->mLastAccelVector.z);
    Log::Info("  mDesiredVelocity[+0x78]=(%.2f, %.2f, %.2f) mPreviousVelocity[+0x84]=(%.2f, %.2f, %.2f) mDesiredFacingVector[+0x90]=(%.3f, %.3f, %.3f)",
              chopper->mDesiredVelocity.x, chopper->mDesiredVelocity.y, chopper->mDesiredVelocity.z, chopper->mPreviousVelocity.x,
              chopper->mPreviousVelocity.y, chopper->mPreviousVelocity.z, chopper->mDesiredFacingVector.x, chopper->mDesiredFacingVector.y,
              chopper->mDesiredFacingVector.z);
    if (const Attrib::Gen::chopperspecs::_LayoutStruct* layout = ChopperSpecs(ichopper))
        Log::Info("  mChopperSpecs[+0x9C].mLayoutPtr=0x%p MAX_SPEED_MPS[+0x44]=%g DRIVE_SPEED[+0x5C]=%g ROLL_ANG[+0x40]=%g PITCH_ANG[+0x38]=%g "
                  "YAW_DAMP[+0x14]=%g AIR_RESISTANCE[+0x20]=%g",
                  layout, layout->MAX_SPEED_MPS, layout->DRIVE_SPEED, layout->ROLL_ANG, layout->PITCH_ANG, layout->YAW_DAMP, layout->AIR_RESISTANCE);
    Log::Info("  Max_Chopper_Accel[0x%08X]=%g Min_Chopper_Accel[0x%08X]=%g", kMax_Chopper_Accel, Game::Global<float>(kMax_Chopper_Accel),
              kMin_Chopper_Accel, Game::Global<float>(kMin_Chopper_Accel));
    Log::Info("  sLive@0x%p totalTorqueYScale=%g angVelYClamp=%g/%g lastAngVelocity=%g*%g lastAccelVector=%g*%g dT>%g", &sLive,
              sLive.totalTorqueYScale, sLive.angVelYClamp, sLive.angVelYClampLow, sLive.lastAngVelocityScale, sLive.lastAngVelocityBlend,
              sLive.lastAccelVectorScale, sLive.lastAccelVectorBlend, sLive.simulateDeltaTimeGate);
}
#endif
