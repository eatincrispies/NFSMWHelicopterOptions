#include <cstddef>
#include <cstdint>
#include "AIActionHeliPursuit.hpp"
#include "Hooks.hpp"
#include "SoundAI.hpp"

namespace {

    constexpr uintptr_t kVTable                       = 0x00891358u;
    constexpr uintptr_t kConstructor                  = 0x00420EC0u;
    constexpr uint8_t   kConstructorPrologue[7]       = { 0x6A, 0xFF, 0x68, 0xF8, 0x7D, 0x86, 0x00 };
    constexpr uintptr_t kSkidHitPursuit               = 0x00412B40u;

    constexpr Patch::FloatOperand kStraightLineLeadBase     = { 0x00412946u, { 0xD8, 0x05 }, 0x00890614u };
    constexpr Patch::FloatOperand kStraightLineLeadMaxTest  = { 0x0041294Cu, { 0xD8, 0x15 }, 0x00890658u };
    constexpr Patch::FloatOperand kStraightLineLeadMaxClamp = { 0x0041295Bu, { 0xD9, 0x05 }, 0x00890658u };
    constexpr Patch::CallSite     kUpdateSkidHitPursuit     = { 0x004278DBu, kSkidHitPursuit };

    constexpr float SkidHitLead  = 0.23f;
    constexpr float StrikeLead   = 0.092f;
    constexpr float StrikeStartD = 4.0f;
    constexpr float StrikeTime   = 2.0f;

    constexpr int kTrackedActions = 4;

    static_assert(offsetof(AIActionHeliPursuit, mIVehicleAI) == 0x4C, "AIActionHeliPursuit::mIVehicleAI");
    static_assert(offsetof(AIActionHeliPursuit, mIRigidBody) == 0x54, "AIActionHeliPursuit::mIRigidBody");
    static_assert(offsetof(AIActionHeliPursuit, mIAIHelicopter) == 0x58, "AIActionHeliPursuit::mIAIHelicopter");
    static_assert(offsetof(AIActionHeliPursuit, mSkidKnockTimer) == 0x64, "AIActionHeliPursuit::mSkidKnockTimer");
    static_assert(offsetof(AIActionHeliPursuit, mSearchDestPoint) == 0x74, "AIActionHeliPursuit::mSearchDestPoint");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerRigidBody) == 0x80, "AIActionHeliPursuit::mPlayerRigidBody");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerPosition) == 0x84, "AIActionHeliPursuit::mPlayerPosition");
    static_assert(offsetof(AIActionHeliPursuit, mSkidHitOffset) == 0x90, "AIActionHeliPursuit::mSkidHitOffset");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerSpeed) == 0xA0, "AIActionHeliPursuit::mPlayerSpeed");
    static_assert(offsetof(AIActionHeliPursuit, mPursuitMode) == 0xA4, "AIActionHeliPursuit::mPursuitMode");

    struct LeadDistance {
        float base;
        float max;
    };

    LeadDistance         gLeadDistance = {};
    AIActionHeliPursuit* gConstructed[kTrackedActions] = {};
    int                  gNextConstructed = 0;
    AIActionHeliPursuit* gCurrent = nullptr;

    bool IsAlive(const AIActionHeliPursuit* action, const IRigidBody* heliRigidBody) {
        uintptr_t vtable = 0;
        const IRigidBody* rigidBody = nullptr;
        const uintptr_t va = reinterpret_cast<uintptr_t>(action);
        return action && Memory::Read(va, &vtable, sizeof(vtable)) && vtable == kVTable
            && Memory::Read(va + offsetof(AIActionHeliPursuit, mIRigidBody), &rigidBody, sizeof(rigidBody)) && rigidBody == heliRigidBody;
    }

    void __cdecl ConstructorEntry(Detour::Registers* registers) {
        auto* action = reinterpret_cast<AIActionHeliPursuit*>(static_cast<uintptr_t>(registers->ecx));
        if (!action) return;
        for (const AIActionHeliPursuit* known : gConstructed)
            if (known == action) return;
        gConstructed[gNextConstructed] = action;
        gNextConstructed = (gNextConstructed + 1) % kTrackedActions;
    }

}

void AIActionHeliPursuit::InstallPatches() {
    Refresh();

    Patch::Begin("AIActionHeliPursuit::StraightLinePursuit");
    Patch::RedirectFloat("leadDist base", kStraightLineLeadBase, &gLeadDistance.base);
    Patch::RedirectFloat("leadDist cap test", kStraightLineLeadMaxTest, &gLeadDistance.max);
    Patch::RedirectFloat("leadDist cap", kStraightLineLeadMaxClamp, &gLeadDistance.max);
    Patch::Commit();

    Patch::Begin("AIActionHeliPursuit::Update");
    Patch::RedirectCall("SkidHitPursuit", kUpdateSkidHitPursuit, Game::MethodAddress(&AIActionHeliPursuit::CrushPursuit));
    Patch::Commit();
}

void AIActionHeliPursuit::Refresh() {
    gLeadDistance.base = gCfg.LeadBase;
    gLeadDistance.max  = gCfg.LeadMax;
}

bool AIActionHeliPursuit::HookConstructor() {
    return Detour::Install("AIActionHeliPursuit::AIActionHeliPursuit", kConstructor, kConstructorPrologue, sizeof(kConstructorPrologue),
                           &ConstructorEntry);
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

void AIActionHeliPursuit::SkidHitPursuit() {
    Game::ThisCall<void>(kSkidHitPursuit, this);
}

void AIActionHeliPursuit::CrushPursuit() {
    SkidHitPursuit();
    if (gCfg.CrushHover <= 0.0f || !IsSkidHitting()) return;

    const UMath::Vector3 myPosition = mIRigidBody->GetPosition();
    const UMath::Vector3 perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    const float lead = mPursuitMode == kSkid_Hit_Approach ? SkidHitLead : StrikeLead;
    UMath::Vector3 seekPosition = mPlayerPosition + perpLinVel * lead;
    const bool overPerp = UMath::Distancexz(myPosition, seekPosition) < StrikeStartD;

    if (mPursuitMode == kSkid_Hit_Approach && overPerp && myPosition.y > mPlayerPosition.y) StartCrush();

    const bool crushing = mPursuitMode == kSkid_Hit_Strike && overPerp;
    seekPosition.y = mPlayerPosition.y + (crushing ? gCfg.CrushHeight : gCfg.CrushHover);
    mIVehicleAI->SetDriveTarget(seekPosition);
}

void AIActionHeliPursuit::StartCrush() {
    mPursuitMode = kSkid_Hit_Strike;
    if (mSkidKnockTimer < StrikeTime) mSkidKnockTimer = StrikeTime;

    SoundAI* copspeech = SoundAI::Get();
    if (copspeech != nullptr && copspeech->GetHeli() != nullptr) copspeech->GetHeli()->IntentToRam();
    Log::Info("Crush attack: the helicopter is over your car and drops onto it.");
}
