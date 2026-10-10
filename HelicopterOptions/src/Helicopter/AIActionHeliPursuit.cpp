#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "AIActionHeliPursuit.hpp"
#include "SoundAI.hpp"

namespace {

    constexpr uintptr_t kVTable = 0x00891358u;

    constexpr float SkidHitLead  = 0.23f;
    constexpr float StrikeLead   = 0.092f;
    constexpr float StrikeStartD = 4.0f;
    constexpr float StrikeTime   = 2.0f;

    constexpr float    kTwoPi                = 6.2831853f;
    constexpr float    kSearchSlowest        = 15.0f;
    constexpr float    kSearchFastest        = 20.0f;
    constexpr float    kSearchNextPointRange = 30.0f;
    constexpr float    kSearchFirstRadius    = 80.0f;
    constexpr float    kSearchRadiusGrowth   = 40.0f;
    constexpr float    kSearchLargestRadius  = 250.0f;
    constexpr float    kSearchRadiusJitter   = 0.25f;
    constexpr float    kSearchSmallestStep   = 0.08f;
    constexpr float    kSearchLargestStep    = 0.17f;
    constexpr float    kSearchHeadingLead    = 0.75f;
    constexpr float    kSearchHeightOverDest = 5.0f;
    constexpr float    kEscapeHeadingSpeed   = 5.0f;
    constexpr float    kLookSweepAngle       = 0.79f;
    constexpr float    kLookSweepPeriod      = 10.0f;
    constexpr float    kLookDistance         = 60.0f;
    constexpr unsigned kSearchDrivingFlags   = 7u;

    constexpr int kTrackedActions = 4;

    static_assert(offsetof(AIActionHeliPursuit, mIVehicleAI) == 0x4C, "AIActionHeliPursuit::mIVehicleAI");
    static_assert(offsetof(AIActionHeliPursuit, mIRigidBody) == 0x54, "AIActionHeliPursuit::mIRigidBody");
    static_assert(offsetof(AIActionHeliPursuit, mIAIHelicopter) == 0x58, "AIActionHeliPursuit::mIAIHelicopter");
    static_assert(offsetof(AIActionHeliPursuit, mPursuitTime) == 0x60, "AIActionHeliPursuit::mPursuitTime");
    static_assert(offsetof(AIActionHeliPursuit, mSkidKnockTimer) == 0x64, "AIActionHeliPursuit::mSkidKnockTimer");
    static_assert(offsetof(AIActionHeliPursuit, mSearchPatternAngle) == 0x70, "AIActionHeliPursuit::mSearchPatternAngle");
    static_assert(offsetof(AIActionHeliPursuit, mSearchDestPoint) == 0x74, "AIActionHeliPursuit::mSearchDestPoint");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerRigidBody) == 0x80, "AIActionHeliPursuit::mPlayerRigidBody");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerPosition) == 0x84, "AIActionHeliPursuit::mPlayerPosition");
    static_assert(offsetof(AIActionHeliPursuit, mSkidHitOffset) == 0x90, "AIActionHeliPursuit::mSkidHitOffset");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerSpeed) == 0xA0, "AIActionHeliPursuit::mPlayerSpeed");
    static_assert(offsetof(AIActionHeliPursuit, mPursuitMode) == 0xA4, "AIActionHeliPursuit::mPursuitMode");

    AIActionHeliPursuit* gConstructed[kTrackedActions] = {};
    int                  gNextConstructed = 0;
    AIActionHeliPursuit* gCurrent = nullptr;
    float                gSearchRadius = kSearchFirstRadius;
    float                gSearchSpeed = kSearchSlowest;
    float                gSearchDirection = 1.0f;
    float                gSearchLapTurned = 0.0f;
    float                gEscapeHeadingX = 0.0f;
    float                gEscapeHeadingZ = 0.0f;
    uint32_t             gRandomState = 0x2545F491u;

    float RandomUnit() {
        gRandomState ^= gRandomState << 13;
        gRandomState ^= gRandomState >> 17;
        gRandomState ^= gRandomState << 5;
        return static_cast<float>(gRandomState & 0xFFFFFFu) / 16777216.0f;
    }

    float RandomRange(float low, float high) {
        return low + (high - low) * RandomUnit();
    }

    bool IsAlive(const AIActionHeliPursuit* action, const IRigidBody* heliRigidBody) {
        uintptr_t vtable = 0;
        const IRigidBody* rigidBody = nullptr;
        const uintptr_t va = reinterpret_cast<uintptr_t>(action);
        return action && Memory::Read(va, &vtable, sizeof(vtable)) && vtable == kVTable
            && Memory::Read(va + offsetof(AIActionHeliPursuit, mIRigidBody), &rigidBody, sizeof(rigidBody)) && rigidBody == heliRigidBody;
    }

}

void AIActionHeliPursuit::Refresh() {
    if (sSettings.LeadBase > sSettings.LeadMax)
        Log::Warn("[Helicopter:Leading] leadBase %g is above leadMax %g, so the helicopter always aims %g m ahead.", sSettings.LeadBase,
                  sSettings.LeadMax, sSettings.LeadMax);
    if (sSettings.CrushHover > 0.0f && sSettings.CrushHeight > sSettings.CrushHover) {
        Log::Warn("[Helicopter:CrushAttack] crushHeight %g is above hoverHeight %g; lowered to match.", sSettings.CrushHeight,
                  sSettings.CrushHover);
        sSettings.CrushHeight = sSettings.CrushHover;
    }

    sLeadDistance.base = sSettings.LeadBase;
    sLeadDistance.max  = sSettings.LeadMax;
}

void __cdecl AIActionHeliPursuit::ConstructorEntry(Detour::Registers* registers) {
    auto* action = reinterpret_cast<AIActionHeliPursuit*>(static_cast<uintptr_t>(registers->ecx));
    if (!action) return;
    for (const AIActionHeliPursuit* known : gConstructed)
        if (known == action) return;
    gConstructed[gNextConstructed] = action;
    gNextConstructed = (gNextConstructed + 1) % kTrackedActions;
}

AIActionHeliPursuit* AIActionHeliPursuit::Find(const IRigidBody* heliRigidBody) {
    if (IsAlive(gCurrent, heliRigidBody)) return gCurrent;
    gCurrent = nullptr;
    for (AIActionHeliPursuit* action : gConstructed) {
        if (IsAlive(action, heliRigidBody)) {
            gCurrent = action;
            break;
        }
    }
    return gCurrent;
}

bool AIActionHeliPursuit::IsCrushing() const {
    return sSettings.CrushHover > 0.0f && IsSkidHitting();
}

void AIActionHeliPursuit::StraightLinePursuit() {
    Game::ThisCall<void>(kStraightLinePursuit, this);
}

void AIActionHeliPursuit::ChasePerp() {
    StraightLinePursuit();
    if (mPlayerRigidBody == nullptr) return;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    const float speed = std::sqrt(perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z);
    if (speed < kEscapeHeadingSpeed) return;
    gEscapeHeadingX = perpLinVel.x / speed;
    gEscapeHeadingZ = perpLinVel.z / speed;
}

void AIActionHeliPursuit::StartSearch() {
    gSearchRadius = kSearchFirstRadius - kSearchRadiusGrowth;
    gSearchLapTurned = 0.0f;
    gSearchDirection = RandomUnit() < 0.5f ? 1.0f : -1.0f;
    SetNextSearchPoint();
}

void AIActionHeliPursuit::SetNextSearchPoint() {
    IPursuit* ip = mIVehicleAI->GetPursuit();
    if (ip == nullptr) return;

    gSearchRadius = std::min(gSearchRadius + kSearchRadiusGrowth, kSearchLargestRadius);
    gSearchSpeed = RandomRange(kSearchSlowest, kSearchFastest);

    const float step = RandomRange(kSearchSmallestStep, kSearchLargestStep);
    gSearchLapTurned += step;
    if (gSearchLapTurned >= 1.0f) {
        gSearchLapTurned -= 1.0f;
        if (RandomUnit() < 0.5f) gSearchDirection = -gSearchDirection;
    }
    mSearchPatternAngle += step * gSearchDirection;
    if (mSearchPatternAngle >= 1.0f) mSearchPatternAngle -= 1.0f;
    if (mSearchPatternAngle < 0.0f) mSearchPatternAngle += 1.0f;

    const UMath::Vector3& lastKnown = ip->GetLastKnownLocation();
    const float lead = gSearchRadius * kSearchHeadingLead;
    const float radius = gSearchRadius * RandomRange(1.0f - kSearchRadiusJitter, 1.0f + kSearchRadiusJitter);
    const float angle = mSearchPatternAngle * kTwoPi;
    mSearchDestPoint = { lastKnown.x + gEscapeHeadingX * lead + std::cos(angle) * radius, lastKnown.y,
                         lastKnown.z + gEscapeHeadingZ * lead + std::sin(angle) * radius };
    mIAIHelicopter->RestrictPointToRoadNet(mSearchDestPoint);
    mIAIHelicopter->FilterHeliAltitude(mSearchDestPoint);
    mSearchDestPoint.y += kSearchHeightOverDest;
}

void AIActionHeliPursuit::SearchForPerp() {
    const UMath::Vector3 myPosition = mIRigidBody->GetPosition();
    if (UMath::Distancexz(myPosition, mSearchDestPoint) < kSearchNextPointRange) SetNextSearchPoint();

    mIVehicleAI->SetDriveSpeed(gSearchSpeed);
    mIAIHelicopter->SetDestinationVelocity(mIRigidBody->GetLinearVelocity());
    mIVehicleAI->SetDriveTarget(mSearchDestPoint);
    mIAIHelicopter->SetLookAtPosition(SearchLookAt(myPosition));
    mIVehicleAI->DoDriving(kSearchDrivingFlags);
}

UMath::Vector3 AIActionHeliPursuit::SearchLookAt(const UMath::Vector3& myPosition) const {
    const float distance = UMath::Distancexz(myPosition, mSearchDestPoint);
    if (distance < 0.1f) return mSearchDestPoint;

    const float forwardX = (mSearchDestPoint.x - myPosition.x) / distance;
    const float forwardZ = (mSearchDestPoint.z - myPosition.z) / distance;
    const float sweep = std::sin(mPursuitTime * kTwoPi / kLookSweepPeriod) * kLookSweepAngle;
    const float c = std::cos(sweep);
    const float s = std::sin(sweep);
    return { myPosition.x + (forwardX * c - forwardZ * s) * kLookDistance, mSearchDestPoint.y,
             myPosition.z + (forwardX * s + forwardZ * c) * kLookDistance };
}

void AIActionHeliPursuit::SkidHitPursuit() {
    Game::ThisCall<void>(kSkidHitPursuit, this);
}

void AIActionHeliPursuit::CrushPursuit() {
    SkidHitPursuit();
    if (!IsCrushing()) return;

    const UMath::Vector3 myPosition = mIRigidBody->GetPosition();
    const UMath::Vector3 perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    const float lead = mPursuitMode == kSkid_Hit_Approach ? SkidHitLead : StrikeLead;
    UMath::Vector3 seekPosition = mPlayerPosition + perpLinVel * lead;
    const bool overPerp = UMath::Distancexz(myPosition, seekPosition) < StrikeStartD;

    if (mPursuitMode == kSkid_Hit_Approach && overPerp && myPosition.y > mPlayerPosition.y) StartCrush();

    const bool crushing = mPursuitMode == kSkid_Hit_Strike && overPerp;
    seekPosition.y = mPlayerPosition.y + (crushing ? sSettings.CrushHeight : sSettings.CrushHover);
    mIVehicleAI->SetDriveTarget(seekPosition);
}

void AIActionHeliPursuit::StartCrush() {
    mPursuitMode = kSkid_Hit_Strike;
    if (mSkidKnockTimer < StrikeTime) mSkidKnockTimer = StrikeTime;

    SoundAI* copspeech = SoundAI::Get();
    if (copspeech != nullptr && copspeech->GetHeli() != nullptr) copspeech->GetHeli()->IntentToRam();
    Log::Info("Crush attack: the helicopter is over your car and drops onto it.");
}
