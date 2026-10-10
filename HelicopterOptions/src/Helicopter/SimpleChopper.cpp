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
    static_assert(offsetof(SimpleChopper, mLastAngVelocity) == 0x60, "SimpleChopper::mLastAngVelocity");
    static_assert(offsetof(SimpleChopper, mDesiredVelocity) == 0x78, "SimpleChopper::mDesiredVelocity");
    static_assert(offsetof(SimpleChopper, mChopperSpecs) == 0x9C, "SimpleChopper::mChopperSpecs");
    static_assert(offsetof(SimpleChopper, mMaxDecelFlag) == 0xC4, "SimpleChopper::mMaxDecelFlag");
    static_assert(offsetof(SimpleChopper, mIrigidBody) == 0xC8, "SimpleChopper::mIrigidBody");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, MAX_SPEED_MPS) == 0x44, "chopperspecs MAX_SPEED_MPS");

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
        return &gSpecs[gSpecsCount++];
    }

}

void SimpleChopper::Refresh() {
    if (sSettings.MinChopperAccel > sSettings.MaxChopperAccel) {
        Log::Warn("[Helicopter:Acceleration] minChopperAccel %g is above maxChopperAccel %g; lowered to match.", sSettings.MinChopperAccel,
                  sSettings.MaxChopperAccel);
        sSettings.MinChopperAccel = sSettings.MaxChopperAccel;
    }

    const Settings gameValues{};
    const float authority = std::fabs(sSettings.TurnResponseScale / gameValues.TurnResponseScale) * (sSettings.TurnClamp / gameValues.TurnClamp);
    if (authority >= kSafeTurningAuthority)
        Log::Warn("Turning authority is %.0f%% of the game's ([Helicopter:Turning] TurnClamp %g x TurnResponseScale %g). "
                  "Past about 175%% the helicopter rolls far over and can flip.",
                  authority * 100.0f, sSettings.TurnClamp, sSettings.TurnResponseScale);

    sLive.totalTorqueYScale = sSettings.TurnResponseScale;
    sLive.angVelYClamp      = sSettings.TurnClamp;
    sLive.angVelYClampLow   = -sSettings.TurnClamp;

    if (sAccelPatched && (sSettings.MaxChopperAccel != sMax_Chopper_Accel || sSettings.MinChopperAccel != sMin_Chopper_Accel)) {
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

    layout->MAX_SPEED_MPS = sSettings.SpeedCap;
    Log::Info("chopperspecs MAX_SPEED_MPS set to %g m/s (%.0f km/h); the game's value is %g m/s.", sSettings.SpeedCap, sSettings.SpeedCap * 3.6f,
              tracked->gameMaxSpeed);
}

void SimpleChopper::RestoreSpeedCap() {
    for (int i = 0; i < gSpecsCount; ++i)
        Memory::WriteData(reinterpret_cast<uintptr_t>(&gSpecs[i].layout->MAX_SPEED_MPS), &gSpecs[i].gameMaxSpeed, sizeof(float));
    gSpecsCount = 0;
}
