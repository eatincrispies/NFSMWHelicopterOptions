#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "SimpleChopper.hpp"
#include "Hooks.hpp"

namespace {

    constexpr uintptr_t kVTableForISimpleChopper = 0x008AB86Cu;
    constexpr uintptr_t kMax_Chopper_Accel       = 0x008F8DCCu;
    constexpr uintptr_t kMin_Chopper_Accel       = 0x008F8DD0u;

    constexpr Patch::FloatOperand kTotalTorqueYScale    = { 0x006A28A8u, { 0xD8, 0x0D }, 0x008AB8ECu };
    constexpr Patch::FloatOperand kAngVelYClampTest     = { 0x006A28BAu, { 0xD8, 0x1D }, 0x008AAE5Cu };
    constexpr Patch::FloatOperand kAngVelYClampHigh     = { 0x006A2994u, { 0xD9, 0x05 }, 0x008AAE5Cu };
    constexpr Patch::FloatOperand kAngVelYClampLowTest  = { 0x006A28CFu, { 0xD9, 0x05 }, 0x008AB8E8u };
    constexpr Patch::FloatOperand kAngVelYClampLow      = { 0x006A28E2u, { 0xD9, 0x05 }, 0x008AB8E8u };
    constexpr Patch::FloatOperand kSimulateDeltaTimeGate = { 0x006A2762u, { 0xD8, 0x1D }, 0x00890EC4u };

    constexpr Patch::FloatOperand kLastAngVelocityScale[3] = {
        { 0x006A28EFu, { 0xD8, 0x0D }, 0x008A0718u },
        { 0x006A28FCu, { 0xD8, 0x0D }, 0x008A0718u },
        { 0x006A2908u, { 0xD8, 0x0D }, 0x008A0718u },
    };
    constexpr Patch::FloatOperand kLastAngVelocityBlend[3] = {
        { 0x006A291Eu, { 0xD8, 0x0D }, 0x00890F14u },
        { 0x006A292Bu, { 0xD8, 0x0D }, 0x00890F14u },
        { 0x006A293Cu, { 0xD8, 0x0D }, 0x00890F14u },
    };
    constexpr Patch::FloatOperand kLastAccelVectorScale[3] = {
        { 0x006A204Fu, { 0xD8, 0x0D }, 0x00890E98u },
        { 0x006A205Bu, { 0xD8, 0x0D }, 0x00890E98u },
        { 0x006A2067u, { 0xD8, 0x0D }, 0x00890E98u },
    };
    constexpr Patch::FloatOperand kLastAccelVectorBlend[3] = {
        { 0x006A2075u, { 0xD8, 0x0D }, 0x00895074u },
        { 0x006A2083u, { 0xD8, 0x0D }, 0x00895074u },
        { 0x006A2092u, { 0xD8, 0x0D }, 0x00895074u },
    };

    constexpr float kGameLastAngVelocityScale = 7.0f;
    constexpr float kGameLastAngVelocityBlend = 0.125f;
    constexpr float kGameLastAccelVectorScale = 4.0f;
    constexpr float kGameLastAccelVectorBlend = 0.2f;
    constexpr float kGameMax_Chopper_Accel    = 80.0f;
    constexpr float kGameMin_Chopper_Accel    = 30.0f;
    constexpr float kLowestDeltaTimeGate      = 0.0001f;
    constexpr int   kTrackedSpecs             = 4;

    static_assert(offsetof(SimpleChopper, mISimpleChopper) == 0x4C, "SimpleChopper's ISimpleChopper");
    static_assert(offsetof(SimpleChopper, mLastAngVelocity) == 0x60, "SimpleChopper::mLastAngVelocity");
    static_assert(offsetof(SimpleChopper, mDesiredVelocity) == 0x78, "SimpleChopper::mDesiredVelocity");
    static_assert(offsetof(SimpleChopper, mChopperSpecs) == 0x9C, "SimpleChopper::mChopperSpecs");
    static_assert(offsetof(SimpleChopper, mMaxDecelFlag) == 0xC4, "SimpleChopper::mMaxDecelFlag");
    static_assert(offsetof(SimpleChopper, mIrigidBody) == 0xC8, "SimpleChopper::mIrigidBody");
    static_assert(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, MAX_SPEED_MPS) == 0x44, "chopperspecs MAX_SPEED_MPS");

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

    struct TrackedSpecs {
        Attrib::Gen::chopperspecs::_LayoutStruct* layout;
        float                                     gameMaxSpeed;
    };

    LiveValues   gLive = {};
    bool         gAccelPatched = false;
    float        gMax_Chopper_Accel = 0.0f;
    float        gMin_Chopper_Accel = 0.0f;
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

void SimpleChopper::InstallPatches() {
    Refresh();
    gLive.lastAngVelocityScale  = kGameLastAngVelocityScale;
    gLive.lastAngVelocityBlend  = kGameLastAngVelocityBlend;
    gLive.lastAccelVectorScale  = kGameLastAccelVectorScale;
    gLive.lastAccelVectorBlend  = kGameLastAccelVectorBlend;
    gLive.simulateDeltaTimeGate = kLowestDeltaTimeGate;

    Patch::Begin("SimpleChopper::OnTaskSimulate");
    Patch::RedirectFloat("angVel.y = -totalTorque.y * 8.0", kTotalTorqueYScale, &gLive.totalTorqueYScale);
    Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) test", kAngVelYClampTest, &gLive.angVelYClamp);
    Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) high", kAngVelYClampHigh, &gLive.angVelYClamp);
    Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) low test", kAngVelYClampLowTest, &gLive.angVelYClampLow);
    Patch::RedirectFloat("UMath::Clamp(angVel.y, -1.3, 1.3) low", kAngVelYClampLow, &gLive.angVelYClampLow);
    for (const Patch::FloatOperand& site : kLastAngVelocityScale)
        Patch::RedirectFloat("UMath::Scale(mLastAngVelocity, 7.0)", site, &gLive.lastAngVelocityScale);
    for (const Patch::FloatOperand& site : kLastAngVelocityBlend)
        Patch::RedirectFloat("UMath::AddScale(angVel, mLastAngVelocity, 0.125)", site, &gLive.lastAngVelocityBlend);
    Patch::RedirectFloat("dT > 0.005", kSimulateDeltaTimeGate, &gLive.simulateDeltaTimeGate);
    Patch::Commit();

    Patch::Begin("SimpleChopper::SetTorqueToMatchPitchAndRoll");
    for (const Patch::FloatOperand& site : kLastAccelVectorScale)
        Patch::RedirectFloat("UMath::Scale(mLastAccelVector, 4.0)", site, &gLive.lastAccelVectorScale);
    for (const Patch::FloatOperand& site : kLastAccelVectorBlend)
        Patch::RedirectFloat("UMath::AddScale(localXZAccel, mLastAccelVector, 0.2)", site, &gLive.lastAccelVectorBlend);
    Patch::Commit();

    Patch::Begin("Max_Chopper_Accel and Min_Chopper_Accel");
    Patch::DataFloat("Max_Chopper_Accel", kMax_Chopper_Accel, kGameMax_Chopper_Accel, gCfg.MaxChopperAccel);
    Patch::DataFloat("Min_Chopper_Accel", kMin_Chopper_Accel, kGameMin_Chopper_Accel, gCfg.MinChopperAccel);
    gAccelPatched = Patch::Commit();
    gMax_Chopper_Accel = gCfg.MaxChopperAccel;
    gMin_Chopper_Accel = gCfg.MinChopperAccel;
}

void SimpleChopper::Refresh() {
    gLive.totalTorqueYScale = gCfg.TurnResponseScale;
    gLive.angVelYClamp      = gCfg.TurnClamp;
    gLive.angVelYClampLow   = -gCfg.TurnClamp;

    if (gAccelPatched && (gCfg.MaxChopperAccel != gMax_Chopper_Accel || gCfg.MinChopperAccel != gMin_Chopper_Accel)) {
        Game::Global<float>(kMax_Chopper_Accel) = gCfg.MaxChopperAccel;
        Game::Global<float>(kMin_Chopper_Accel) = gCfg.MinChopperAccel;
        gMax_Chopper_Accel = gCfg.MaxChopperAccel;
        gMin_Chopper_Accel = gCfg.MinChopperAccel;
    }
}

void SimpleChopper::ScaleMotionFilters(float frames) {
    ScaleFilterPair(kGameLastAngVelocityScale, kGameLastAngVelocityBlend, frames, &gLive.lastAngVelocityScale, &gLive.lastAngVelocityBlend);
    ScaleFilterPair(kGameLastAccelVectorScale, kGameLastAccelVectorBlend, frames, &gLive.lastAccelVectorScale, &gLive.lastAccelVectorBlend);
}

void SimpleChopper::ApplySpeedCap(ISimpleChopper* ichopper) {
    Attrib::Gen::chopperspecs::_LayoutStruct* layout = ChopperSpecs(ichopper);
    if (!layout) return;
    const TrackedSpecs* tracked = Track(layout);
    if (!tracked || layout->MAX_SPEED_MPS == gCfg.SpeedCap || !Memory::IsFinite(gCfg.SpeedCap)) return;

    layout->MAX_SPEED_MPS = gCfg.SpeedCap;
    Log::Info("chopperspecs MAX_SPEED_MPS set to %g m/s (%.0f km/h); the game's value is %g m/s.", gCfg.SpeedCap, gCfg.SpeedCap * 3.6f,
              tracked->gameMaxSpeed);
}

void SimpleChopper::RestoreSpeedCap() {
    for (int i = 0; i < gSpecsCount; ++i)
        Memory::WriteData(reinterpret_cast<uintptr_t>(&gSpecs[i].layout->MAX_SPEED_MPS), &gSpecs[i].gameMaxSpeed, sizeof(float));
    gSpecsCount = 0;
}
