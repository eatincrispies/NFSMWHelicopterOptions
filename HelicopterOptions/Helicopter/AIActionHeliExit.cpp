#include <cmath>
#include <cstddef>
#include "AIActionHeliExit.hpp"
#include "AIActionHeliPursuit.hpp"
#include "AIVehicleHelicopter.hpp"

namespace {

    constexpr float kExitHeight   = 30.0f;
    constexpr float kFlyOutLead   = 200.0f;
    constexpr float kTooCloseToTurn = 1.0f;

    static_assert(offsetof(AIActionHeliExit, mIVehicleAI) == 0x48, "AIActionHeliExit::mIVehicleAI");
    static_assert(offsetof(AIActionHeliExit, mIRigidBody) == 0x50, "AIActionHeliExit::mIRigidBody");
    static_assert(offsetof(AIActionHeliExit, mIAIHelicopter) == 0x54, "AIActionHeliExit::mIAIHelicopter");
    static_assert(offsetof(AIActionHeliExit, mExitTime) == 0x5C, "AIActionHeliExit::mExitTime");
    static_assert(offsetof(AIActionHeliExit, mSeekPosition) == 0x64, "AIActionHeliExit::mSeekPosition");
    static_assert(offsetof(AIActionHeliExit, mExitMode) == 0x70, "AIActionHeliExit::mExitMode");

    UMath::Vector3 FlatUnit(UMath::Vector3 v) {
        v.y = 0.0f;
        const float length = std::sqrt(v.x * v.x + v.z * v.z);
        if (length < kTooCloseToTurn) return { 0.0f, 0.0f, 0.0f };
        return { v.x / length, 0.0f, v.z / length };
    }

}

void AIActionHeliExit::Refresh() {
    if (!sPatched || sSettings.FlySpeed == sFlySpeed) return;
    if (!Patch::RewritePushedFloat("AIActionHeliExit::Update | flySpeed", kUpdateFlySpeed, sFlySpeed, sSettings.FlySpeed)) {
        sPatched = false;
        return;
    }
    sFlySpeed = sSettings.FlySpeed;
}

void AIActionHeliExit::FlyAway(float dT) {
    mExitTime += dT;

    const UMath::Vector3 myPosition = mIRigidBody->GetPosition();
    IRigidBody* playerRigidBody = GetLocalPlayerRigidBody();
    const UMath::Vector3 playerPosition = playerRigidBody != nullptr ? playerRigidBody->GetPosition() : myPosition;

    if (mExitMode != kFlyOut) {
        UMath::Vector3 heading = FlatUnit(myPosition - playerPosition);
        if (heading.x == 0.0f && heading.z == 0.0f) {
            UMath::Vector3 forward = {};
            mIRigidBody->GetForwardVector(forward);
            heading = FlatUnit(forward * 100.0f);
        }
        if (heading.x == 0.0f && heading.z == 0.0f) heading = { 1.0f, 0.0f, 0.0f };
        mSeekPosition = heading;
        mExitMode = kFlyOut;
        Log::Info("AIActionHeliExit 0x%p  FlyAway  heading=(%.3f, %.3f) pos=(%.1f, %.1f, %.1f) player=(%.1f, %.1f, %.1f)", this, heading.x, heading.z,
                  myPosition.x, myPosition.y, myPosition.z, playerPosition.x, playerPosition.y, playerPosition.z);
    }

    UMath::Vector3 target = myPosition + mSeekPosition * kFlyOutLead;
    target.y = playerPosition.y + kExitHeight;

    mIAIHelicopter->SetLookAtPosition(target);
    mIVehicleAI->SetDriveSpeed(sSettings.FlySpeed);
    mIAIHelicopter->SetDestinationVelocity(mIRigidBody->GetLinearVelocity());
    mIVehicleAI->SetDriveTarget(target);
    mIVehicleAI->DoDriving(7);
}
