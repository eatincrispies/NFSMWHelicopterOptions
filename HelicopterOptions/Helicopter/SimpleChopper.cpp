#include <math.h>
#include "SimpleChopper.hpp"
#include "HeliSheet.hpp"

namespace {

    #define VTABLE_FOR_ISIMPLECHOPPER        0x008AB86C
    #define ON_TASK_SIMULATE                 0x006A2430
    #define VTABLE_ON_TASK_SIMULATE          0x008AB8A8
    #define MAX_CHOPPER_ACCEL                0x008F8DCC
    #define MIN_CHOPPER_ACCEL                0x008F8DD0

    #define IDAMAGEABLE_IS_DESTROYED         0x018
    #define RIGID_BODY_SPECS_LAYOUT          0x03C
    #define RIGID_BODY_SPECS_SIZE            0x101
    #define SPECS_GROUND_FRICTION            0x0E8
    #define SPECS_NO_WORLD_COLLISIONS        0x0FE
    #define WRECK_STATIC_FRICTION_PERCENT    80
    #define WRECK_SLIDING_FRICTION_PERCENT   120
    #define WRECK_SPECS_SIZE                 0x110
    #define MOST_WRECKS                      8

    #define LEVELING_RATE                    3
    #define NEAR_GROUND_LEVELING_RATE        6
    #define FALLING_SPEED                    3
    #define LANDED_SPEED                     1
    #define NOT_FALLING_SECONDS              1
    #define LONGEST_FALL_SECONDS             6

    #define CLIMB_SLOWEST                    8
    #define CLIMB_SLOPE_PERCENT              40
    #define ESCAPE_CLIMB                     25

    #define REFERENCE_FPS                    60
    #define TRACKED_SPECS                    4
    #define MOST_FILTER_FRAMES               64
    #define MOST_RETAINED_PER_MILLION        999999
    #define SLOWEST_SPEC_SPEED               1
    #define FASTEST_SPEC_SPEED               10000
    #define LOWEST_DELTA_TIME_US             100
    #define MICROSECONDS_PER_SECOND          1000000

    #define GAME_LAST_ANG_VELOCITY_SCALE     7.0f
    #define GAME_LAST_ANG_VELOCITY_BLEND     0.125f
    #define GAME_MAX_CHOPPER_ACCEL           80.0f
    #define GAME_MIN_CHOPPER_ACCEL           30.0f

    typedef void (__thiscall* SimulateMethod)(void* chopper, float dT);

    const Patch::FloatOperand kTotalTorqueYScale     = { 0x006A28A8, { 0xD8, 0x0D }, 0x008AB8EC };
    const Patch::FloatOperand kAngVelYClampTest      = { 0x006A28BA, { 0xD8, 0x1D }, 0x008AAE5C };
    const Patch::FloatOperand kAngVelYClampHigh      = { 0x006A2994, { 0xD9, 0x05 }, 0x008AAE5C };
    const Patch::FloatOperand kAngVelYClampLowTest   = { 0x006A28CF, { 0xD9, 0x05 }, 0x008AB8E8 };
    const Patch::FloatOperand kAngVelYClampLow       = { 0x006A28E2, { 0xD9, 0x05 }, 0x008AB8E8 };
    const Patch::FloatOperand kSimulateDeltaTimeGate = { 0x006A2762, { 0xD8, 0x1D }, 0x00890EC4 };

    const Patch::FloatOperand kLastAngVelocityScale[] = {
        { 0x006A28EF, { 0xD8, 0x0D }, 0x008A0718 },
        { 0x006A28FC, { 0xD8, 0x0D }, 0x008A0718 },
        { 0x006A2908, { 0xD8, 0x0D }, 0x008A0718 },
    };

    const Patch::FloatOperand kLastAngVelocityBlend[] = {
        { 0x006A291E, { 0xD8, 0x0D }, 0x00890F14 },
        { 0x006A292B, { 0xD8, 0x0D }, 0x00890F14 },
        { 0x006A293C, { 0xD8, 0x0D }, 0x00890F14 },
    };

    #define FILTER_SITES                     ((int)(sizeof(kLastAngVelocityScale) / sizeof(kLastAngVelocityScale[0])))

    C_ASSERT(offsetof(SimpleChopper, mISimpleChopper) == 0x4C);
    C_ASSERT(offsetof(SimpleChopper, mLastBodyOffset) == 0x54);
    C_ASSERT(offsetof(SimpleChopper, mLastAngVelocity) == 0x60);
    C_ASSERT(offsetof(SimpleChopper, mLastAccelVector) == 0x6C);
    C_ASSERT(offsetof(SimpleChopper, mDesiredVelocity) == 0x78);
    C_ASSERT(offsetof(SimpleChopper, mPreviousVelocity) == 0x84);
    C_ASSERT(offsetof(SimpleChopper, mDesiredFacingVector) == 0x90);
    C_ASSERT(offsetof(SimpleChopper, mChopperSpecs) == 0x9C);
    C_ASSERT(offsetof(SimpleChopper, mMaxDecelFlag) == 0xC4);
    C_ASSERT(offsetof(SimpleChopper, mIrigidBody) == 0xC8);
    C_ASSERT(offsetof(Attrib::Instance, mLayoutPtr) == 0x08);
    C_ASSERT(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, YAW_DAMP) == 0x14);
    C_ASSERT(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, AIR_RESISTANCE) == 0x20);
    C_ASSERT(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, PITCH_ANG) == 0x38);
    C_ASSERT(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, ROLL_ANG) == 0x40);
    C_ASSERT(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, MAX_SPEED_MPS) == 0x44);
    C_ASSERT(offsetof(Attrib::Gen::chopperspecs::_LayoutStruct, DRIVE_SPEED) == 0x5C);

    struct LiveValues {
        float totalTorqueYScale;
        float angVelYClamp;
        float angVelYClampLow;
        float lastAngVelocityScale;
        float lastAngVelocityBlend;
        float simulateDeltaTimeGate;
    };

    struct TrackedSpecs {
        Attrib::Gen::chopperspecs::_LayoutStruct* layout;
        float                                     gameMaxSpeed;
    };

    struct Wreck {
        __declspec(align(16)) BYTE specs[WRECK_SPECS_SIZE];
        IRigidBody*                body;
        DWORD                      original;
        float                      time;
        bool                       falling;
        bool                       landed;
    };

    LiveValues   gLive;
    bool         gAccelPatched    = false;
    float        gMaxChopperAccel = 0;
    float        gMinChopperAccel = 0;
    TrackedSpecs gSpecs[TRACKED_SPECS];
    int          gSpecsCount      = 0;
    Wreck        gWrecks[MOST_WRECKS];

    void                                      ScaleFilterPair(float scale, float blend, float frames, float* outScale, float* outBlend);
    SimpleChopper*                            FromISimpleChopper(ISimpleChopper* ichopper);
    Attrib::Gen::chopperspecs::_LayoutStruct* ChopperSpecs(ISimpleChopper* ichopper);
    const TrackedSpecs*                       Track(Attrib::Gen::chopperspecs::_LayoutStruct* layout);
    DWORD                                     SpecsPointer(const IRigidBody* body);
    Wreck*                                    FindWreck(const IRigidBody* body);
    void                                      Release(Wreck& wreck);
    Wreck*                                    BeginWreck(IRigidBody* body);
    void                                      Fall(Wreck& wreck, float dT);

}

SimpleChopper::Settings SimpleChopper::sSettings;

SimpleChopper::Settings::Settings()
    : TurnClamp(1.3f),
      TurnResponseScale(-8.0f),
      MaxChopperAccel(80.0f),
      MinChopperAccel(30.0f),
      SpeedCap(100.0f) {
}

void SimpleChopper::InstallPatches() {
    Refresh();
    gLive.lastAngVelocityScale = GAME_LAST_ANG_VELOCITY_SCALE;
    gLive.lastAngVelocityBlend = GAME_LAST_ANG_VELOCITY_BLEND;
    gLive.simulateDeltaTimeGate = (float)LOWEST_DELTA_TIME_US / MICROSECONDS_PER_SECOND;

    Patch::Begin();
    Patch::RedirectFloat(kTotalTorqueYScale, &gLive.totalTorqueYScale);
    Patch::RedirectFloat(kAngVelYClampTest, &gLive.angVelYClamp);
    Patch::RedirectFloat(kAngVelYClampHigh, &gLive.angVelYClamp);
    Patch::RedirectFloat(kAngVelYClampLowTest, &gLive.angVelYClampLow);
    Patch::RedirectFloat(kAngVelYClampLow, &gLive.angVelYClampLow);
    for (int i = 0; i < FILTER_SITES; ++i) Patch::RedirectFloat(kLastAngVelocityScale[i], &gLive.lastAngVelocityScale);
    for (int i = 0; i < FILTER_SITES; ++i) Patch::RedirectFloat(kLastAngVelocityBlend[i], &gLive.lastAngVelocityBlend);
    Patch::RedirectFloat(kSimulateDeltaTimeGate, &gLive.simulateDeltaTimeGate);
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectVirtual(VTABLE_ON_TASK_SIMULATE, ON_TASK_SIMULATE, MethodAddress(&SimpleChopper::OnTaskSimulate));
    Patch::Commit();

    Patch::Begin();
    Patch::DataFloat(MAX_CHOPPER_ACCEL, GAME_MAX_CHOPPER_ACCEL, sSettings.MaxChopperAccel);
    Patch::DataFloat(MIN_CHOPPER_ACCEL, GAME_MIN_CHOPPER_ACCEL, sSettings.MinChopperAccel);
    gAccelPatched = Patch::Commit();
    gMaxChopperAccel = sSettings.MaxChopperAccel;
    gMinChopperAccel = sSettings.MinChopperAccel;
}

void SimpleChopper::Refresh() {
    if (sSettings.MinChopperAccel > sSettings.MaxChopperAccel) sSettings.MinChopperAccel = sSettings.MaxChopperAccel;

    gLive.totalTorqueYScale = sSettings.TurnResponseScale;
    gLive.angVelYClamp = sSettings.TurnClamp;
    gLive.angVelYClampLow = -sSettings.TurnClamp;

    if (gAccelPatched && (sSettings.MaxChopperAccel != gMaxChopperAccel || sSettings.MinChopperAccel != gMinChopperAccel)) {
        *(float*)MAX_CHOPPER_ACCEL = sSettings.MaxChopperAccel;
        *(float*)MIN_CHOPPER_ACCEL = sSettings.MinChopperAccel;
        gMaxChopperAccel = sSettings.MaxChopperAccel;
        gMinChopperAccel = sSettings.MinChopperAccel;
    }
}

void SimpleChopper::RestoreWrecks() {
    for (int i = 0; i < MOST_WRECKS; ++i)
        if (gWrecks[i].body != NULL) Release(gWrecks[i]);
}

bool SimpleChopper::IsWreck(ISimpleChopper* ichopper) {
    const SimpleChopper* chopper = FromISimpleChopper(ichopper);
    return chopper != NULL && chopper->IsDestroyed();
}

void SimpleChopper::OnTaskSimulate(float dT) {
    if (IsDestroyed()) {
        Crash(dT);
        return;
    }
    Recover();
    ScaleFilterPair(GAME_LAST_ANG_VELOCITY_SCALE, GAME_LAST_ANG_VELOCITY_BLEND, dT * REFERENCE_FPS, &gLive.lastAngVelocityScale, &gLive.lastAngVelocityBlend);
    ((SimulateMethod)ON_TASK_SIMULATE)(this, dT);
    LimitClimb();
    KeepLevelNearGround();
}

void SimpleChopper::KeepLevelNearGround() {
    if (mIrigidBody == NULL || !HeliSheet::TiltedNearGround(mIrigidBody)) return;
    UMath::Vector3 up = UMath::Vector(0, 0, 0);
    mIrigidBody->GetUpVector(up);
    UMath::Vector3 spin = mIrigidBody->GetAngularVelocity();
    UMath::Vector3 level = up * UMath::Dot(spin, up) + UMath::Cross(up, UMath::Vector(0, 1, 0)) * NEAR_GROUND_LEVELING_RATE;
    if (!UMath::IsFinite(level)) return;
    mIrigidBody->SetAngularVelocity(level);
}

void SimpleChopper::LimitClimb() {
    if (mIrigidBody == NULL) return;
    UMath::Vector3 velocity = mIrigidBody->GetLinearVelocity();
    if (!UMath::IsFinite(velocity)) return;
    float limit = ClimbLimit(mIrigidBody->GetPosition(), sqrtf(velocity.x * velocity.x + velocity.z * velocity.z));
    if (!(velocity.y > limit)) return;
    velocity.y = limit;
    mIrigidBody->SetLinearVelocity(velocity);
}

float SimpleChopper::ClimbLimit(const UMath::Vector3& position, float flatSpeed) {
    if (HeliSheet::InsideSolid(position)) return ESCAPE_CLIMB;
    return Larger(CLIMB_SLOWEST, flatSpeed * Fraction(CLIMB_SLOPE_PERCENT));
}

bool SimpleChopper::IsDestroyed() const {
    return mIdamage != NULL && ((BoolQuery)VirtualFunction(mIdamage, IDAMAGEABLE_IS_DESTROYED))(mIdamage);
}

void SimpleChopper::Crash(float dT) {
    if (mIrigidBody == NULL) return;
    Wreck* wreck = FindWreck(mIrigidBody);
    if (wreck == NULL) wreck = BeginWreck(mIrigidBody);
    if (wreck != NULL) Fall(*wreck, dT);
}

void SimpleChopper::Recover() {
    Wreck* wreck = mIrigidBody != NULL ? FindWreck(mIrigidBody) : NULL;
    if (wreck != NULL) Release(*wreck);
}

void SimpleChopper::ApplySpeedCap(ISimpleChopper* ichopper) {
    Attrib::Gen::chopperspecs::_LayoutStruct* layout = ChopperSpecs(ichopper);
    if (layout == NULL) return;
    const TrackedSpecs* tracked = Track(layout);
    if (tracked == NULL || layout->MAX_SPEED_MPS == sSettings.SpeedCap || !Memory::IsFinite(sSettings.SpeedCap)) return;
    layout->MAX_SPEED_MPS = sSettings.SpeedCap;
}

void SimpleChopper::RestoreSpeedCap() {
    for (int i = 0; i < gSpecsCount; ++i) Memory::WriteData((DWORD)&gSpecs[i].layout->MAX_SPEED_MPS, &gSpecs[i].gameMaxSpeed, sizeof(float));
    gSpecsCount = 0;
}

namespace {

    void ScaleFilterPair(float scale, float blend, float frames, float* outScale, float* outBlend) {
        *outScale = scale;
        *outBlend = blend;

        float retained = scale * blend;
        float steps = Memory::IsFinite(frames) ? Clamp(frames, 0, MOST_FILTER_FRAMES) : 1;
        if (!(retained > 0) || retained >= 1 || steps <= 0 || steps == 1) return;

        float retainedNow = powf(retained, steps);
        if (!Memory::IsFinite(retainedNow) || retainedNow <= 0 || retainedNow >= (float)MOST_RETAINED_PER_MILLION / MICROSECONDS_PER_SECOND) return;

        float newBlend = 1 - retainedNow;
        *outBlend = newBlend;
        *outScale = retainedNow / newBlend;
    }

    SimpleChopper* FromISimpleChopper(ISimpleChopper* ichopper) {
        DWORD vtable = 0;
        if (ichopper == NULL || !Memory::Read((DWORD)ichopper, &vtable, sizeof(vtable)) || vtable != VTABLE_FOR_ISIMPLECHOPPER) return NULL;
        return (SimpleChopper*)((DWORD)ichopper - offsetof(SimpleChopper, mISimpleChopper));
    }

    Attrib::Gen::chopperspecs::_LayoutStruct* ChopperSpecs(ISimpleChopper* ichopper) {
        SimpleChopper* chopper = FromISimpleChopper(ichopper);
        Attrib::Gen::chopperspecs::_LayoutStruct* layout = chopper != NULL ? chopper->mChopperSpecs.GetLayout() : NULL;
        if (layout == NULL) return NULL;
        float speed = layout->MAX_SPEED_MPS;
        return Memory::IsFinite(speed) && speed >= SLOWEST_SPEC_SPEED && speed <= FASTEST_SPEC_SPEED ? layout : NULL;
    }

    const TrackedSpecs* Track(Attrib::Gen::chopperspecs::_LayoutStruct* layout) {
        for (int i = 0; i < gSpecsCount; ++i)
            if (gSpecs[i].layout == layout) return &gSpecs[i];
        if (gSpecsCount == TRACKED_SPECS) return NULL;
        gSpecs[gSpecsCount].layout = layout;
        gSpecs[gSpecsCount].gameMaxSpeed = layout->MAX_SPEED_MPS;
        return &gSpecs[gSpecsCount++];
    }

    DWORD SpecsPointer(const IRigidBody* body) {
        DWORD specs = 0;
        Memory::Read((DWORD)body + RIGID_BODY_SPECS_LAYOUT, &specs, sizeof(specs));
        return specs;
    }

    Wreck* FindWreck(const IRigidBody* body) {
        for (int i = 0; i < MOST_WRECKS; ++i) {
            Wreck& wreck = gWrecks[i];
            if (wreck.body != NULL && SpecsPointer(wreck.body) != (DWORD)wreck.specs) {
                wreck.body = NULL;
                wreck.original = 0;
            }
            if (wreck.body == body) return &wreck;
        }
        return NULL;
    }

    void Release(Wreck& wreck) {
        if (SpecsPointer(wreck.body) == (DWORD)wreck.specs) Memory::WriteData((DWORD)wreck.body + RIGID_BODY_SPECS_LAYOUT, &wreck.original, sizeof(wreck.original));
        wreck.body = NULL;
        wreck.original = 0;
    }

    Wreck* BeginWreck(IRigidBody* body) {
        Wreck* wreck = FindWreck(NULL);
        DWORD original = SpecsPointer(body);
        if (wreck == NULL || original == 0 || !Memory::Read(original, wreck->specs, RIGID_BODY_SPECS_SIZE)) return NULL;

        wreck->specs[SPECS_NO_WORLD_COLLISIONS] = false;
        float friction[2] = { Fraction(WRECK_STATIC_FRICTION_PERCENT), Fraction(WRECK_SLIDING_FRICTION_PERCENT) };
        memcpy(wreck->specs + SPECS_GROUND_FRICTION, friction, sizeof(friction));
        DWORD specs = (DWORD)wreck->specs;
        if (!Memory::WriteData((DWORD)body + RIGID_BODY_SPECS_LAYOUT, &specs, sizeof(specs))) return NULL;
        wreck->body = body;
        wreck->original = original;
        wreck->time = 0;
        wreck->falling = false;
        wreck->landed = false;
        return wreck;
    }

    void Fall(Wreck& wreck, float dT) {
        if (wreck.landed) return;
        const UMath::Vector3& velocity = wreck.body->GetLinearVelocity();
        if (velocity.y < -FALLING_SPEED) wreck.falling = true;
        wreck.time += dT;
        wreck.landed = (wreck.falling && velocity.y > -LANDED_SPEED) || (!wreck.falling && wreck.time > NOT_FALLING_SECONDS) || wreck.time > LONGEST_FALL_SECONDS;

        UMath::Vector3 up = UMath::Vector(0, 0, 0);
        wreck.body->GetUpVector(up);
        UMath::Vector3 level = wreck.landed ? UMath::Vector(0, 0, 0) : UMath::Cross(up, UMath::Vector(0, 1, 0)) * LEVELING_RATE;
        wreck.body->SetAngularVelocity(level);
    }
}
