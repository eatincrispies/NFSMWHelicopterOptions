#include <math.h>
#include "AIActionHeliExit.hpp"
#include "AIActionHeliPursuit.hpp"
#include "AIVehicleHelicopter.hpp"

namespace {

    #define HELI_EXIT_UPDATE                 0x004279F0
    #define HELI_EXIT_VTABLE_UPDATE          0x008913A4
    #define HELI_EXIT_FLY_SPEED_PUSH         0x00427BFE
    #define GAME_FLY_SPEED                   100

    #define EXIT_HEIGHT                      30
    #define FLY_OUT_LEAD                     200
    #define TOO_CLOSE_TO_TURN                1
    #define FORWARD_REACH                    100
    #define EXIT_DRIVING_FLAGS               7

    const Patch::FloatPush kUpdateFlySpeed = { HELI_EXIT_FLY_SPEED_PUSH, GAME_FLY_SPEED };

    C_ASSERT(offsetof(AIActionHeliExit, mIVehicleAI) == 0x48);
    C_ASSERT(offsetof(AIActionHeliExit, mIRigidBody) == 0x50);
    C_ASSERT(offsetof(AIActionHeliExit, mIAIHelicopter) == 0x54);
    C_ASSERT(offsetof(AIActionHeliExit, mExitTime) == 0x5C);
    C_ASSERT(offsetof(AIActionHeliExit, mSeekPosition) == 0x64);
    C_ASSERT(offsetof(AIActionHeliExit, mExitMode) == 0x70);

    bool  gPatched  = false;
    float gFlySpeed = 0;

    UMath::Vector3 FlatUnit(UMath::Vector3 v);

}

AIActionHeliExit::Settings AIActionHeliExit::sSettings;

AIActionHeliExit::Settings::Settings()
    : FlySpeed(100.0f) {
}

void AIActionHeliExit::InstallPatches() {
    Patch::Begin();
    Patch::PushFloat(kUpdateFlySpeed, sSettings.FlySpeed);
    gPatched = Patch::Commit();
    gFlySpeed = sSettings.FlySpeed;

    Patch::Begin();
    Patch::RedirectVirtual(HELI_EXIT_VTABLE_UPDATE, HELI_EXIT_UPDATE, MethodAddress(&AIActionHeliExit::FlyAway));
    Patch::Commit();
}

void AIActionHeliExit::Refresh() {
    if (!gPatched || sSettings.FlySpeed == gFlySpeed) return;
    if (!Patch::RewritePushedFloat(kUpdateFlySpeed, gFlySpeed, sSettings.FlySpeed)) {
        gPatched = false;
        return;
    }
    gFlySpeed = sSettings.FlySpeed;
}

void AIActionHeliExit::FlyAway(float dT) {
    mExitTime += dT;

    UMath::Vector3 myPosition = mIRigidBody->GetPosition();
    IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
    UMath::Vector3 playerPosition = playerRigidBody != NULL ? playerRigidBody->GetPosition() : myPosition;

    if (mExitMode != kFlyOut) {
        UMath::Vector3 heading = FlatUnit(myPosition - playerPosition);
        if (heading.x == 0 && heading.z == 0) {
            UMath::Vector3 forward = UMath::Vector(0, 0, 0);
            mIRigidBody->GetForwardVector(forward);
            heading = FlatUnit(forward * FORWARD_REACH);
        }
        if (heading.x == 0 && heading.z == 0) heading = UMath::Vector(1, 0, 0);
        mSeekPosition = heading;
        mExitMode = kFlyOut;
    }

    UMath::Vector3 target = myPosition + mSeekPosition * FLY_OUT_LEAD;
    target.y = playerPosition.y + EXIT_HEIGHT;

    mIAIHelicopter->SetLookAtPosition(target);
    mIVehicleAI->SetDriveSpeed(sSettings.FlySpeed);
    mIAIHelicopter->SetDestinationVelocity(mIRigidBody->GetLinearVelocity());
    mIVehicleAI->SetDriveTarget(target);
    mIVehicleAI->DoDriving(EXIT_DRIVING_FLAGS);
}

namespace {

    UMath::Vector3 FlatUnit(UMath::Vector3 v) {
        v.y = 0;
        float length = sqrtf(v.x * v.x + v.z * v.z);
        if (length < TOO_CLOSE_TO_TURN) return UMath::Vector(0, 0, 0);
        return UMath::Vector(v.x / length, 0, v.z / length);
    }

}
