#pragma once
#include "AIPerpVehicle.hpp"

class IPursuitAI;

class AIActionHeliPursuit {
  public:
    enum kPursuitMode {
        kStraight_Line     = 0,
        kSearch_Pattern    = 1,
        kSkid_Hit_Approach = 2,
        kSkid_Hit_Strike   = 3,
    };

    struct Settings {
        float LeadBase    = 30.0f;
        float LeadMax     = 45.0f;
        float CrushHover  = 0.0f;
        float CrushHeight = 0.0f;
    };

    static inline Settings sSettings;

    static inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:Leading",     "LeadBase",    &sSettings.LeadBase,    nullptr, 0.0f, 100.0f, true },
        { "Helicopter:Leading",     "LeadMax",     &sSettings.LeadMax,     nullptr, 5.0f, 150.0f, true },
        { "Helicopter:CrushAttack", "HoverHeight", &sSettings.CrushHover,  nullptr, 0.0f, 10.0f,  false },
        { "Helicopter:CrushAttack", "CrushHeight", &sSettings.CrushHeight, nullptr, 0.0f, 6.0f,   false },
    };

    static constexpr uintptr_t kConstructor            = 0x00420EC0u;
    static constexpr uint8_t   kConstructorPrologue[7] = { 0x6A, 0xFF, 0x68, 0xF8, 0x7D, 0x86, 0x00 };
    static constexpr uintptr_t kStraightLinePursuit    = 0x00412770u;
    static constexpr uintptr_t kSkidHitPursuit         = 0x00412B40u;
    static constexpr uintptr_t kSetNextPerpSearchDest  = 0x00412F30u;
    static constexpr uintptr_t kSearchForPerp          = 0x00413090u;

    static constexpr Patch::FloatOperand kStraightLineLeadBase      = { 0x00412946u, { 0xD8, 0x05 }, 0x00890614u };
    static constexpr Patch::FloatOperand kStraightLineLeadMaxTest   = { 0x0041294Cu, { 0xD8, 0x15 }, 0x00890658u };
    static constexpr Patch::FloatOperand kStraightLineLeadMaxClamp  = { 0x0041295Bu, { 0xD9, 0x05 }, 0x00890658u };
    static constexpr Patch::CallSite     kUpdateStraightLinePursuit = { 0x004278C0u, kStraightLinePursuit };
    static constexpr Patch::CallSite     kUpdateSkidHitPursuit      = { 0x004278DBu, kSkidHitPursuit };
    static constexpr Patch::CallSite     kUpdateStartSearch         = { 0x00427881u, kSetNextPerpSearchDest };
    static constexpr Patch::CallSite     kUpdateSearchForPerp       = { 0x004278E6u, kSearchForPerp };

    struct LeadDistance {
        float base;
        float max;
    };

    static inline LeadDistance sLeadDistance = {};

    static void InstallPatches() {
        Refresh();

        Patch::Begin("AIActionHeliPursuit::StraightLinePursuit");
        Patch::RedirectFloat("leadDist base", kStraightLineLeadBase, &sLeadDistance.base);
        Patch::RedirectFloat("leadDist cap test", kStraightLineLeadMaxTest, &sLeadDistance.max);
        Patch::RedirectFloat("leadDist cap", kStraightLineLeadMaxClamp, &sLeadDistance.max);
        Patch::Commit();

        Patch::Begin("AIActionHeliPursuit::Update");
        Patch::RedirectCall("SkidHitPursuit", kUpdateSkidHitPursuit, Game::MethodAddress(&AIActionHeliPursuit::CrushPursuit));
        Patch::Commit();

        Patch::Begin("AIActionHeliPursuit::Update kSearch_Pattern");
        Patch::RedirectCall("StraightLinePursuit", kUpdateStraightLinePursuit, Game::MethodAddress(&AIActionHeliPursuit::ChasePerp));
        Patch::RedirectCall("SetNextPerpSearchDest", kUpdateStartSearch, Game::MethodAddress(&AIActionHeliPursuit::StartSearch));
        Patch::RedirectCall("SearchForPerp", kUpdateSearchForPerp, Game::MethodAddress(&AIActionHeliPursuit::SearchForPerp));
        Patch::Commit();
    }

    static bool HookConstructor() {
        return Detour::Install("AIActionHeliPursuit::AIActionHeliPursuit", kConstructor, kConstructorPrologue, sizeof(kConstructorPrologue),
                               &ConstructorEntry);
    }

    static void Refresh();
    static void __cdecl ConstructorEntry(Detour::Registers* registers);
    static AIActionHeliPursuit* Find(const IRigidBody* heliRigidBody);

    bool IsSkidHitting() const {
        return mPursuitMode == kSkid_Hit_Approach || mPursuitMode == kSkid_Hit_Strike;
    }

    bool IsCrushing() const;
    void StraightLinePursuit();
    void ChasePerp();
    void StartSearch();
    void SetNextSearchPoint();
    void SearchForPerp();
    UMath::Vector3 SearchLookAt(const UMath::Vector3& myPosition) const;
    void SkidHitPursuit();
    void CrushPursuit();
    void StartCrush();

    unsigned char  mAIAction[0x4C];
    IVehicleAI*    mIVehicleAI;
    IVehicle*      mIVehicle;
    IRigidBody*    mIRigidBody;
    IAIHelicopter* mIAIHelicopter;
    IPursuitAI*    mIPursuitAI;
    float          mPursuitTime;
    float          mSkidKnockTimer;
    float          mPathTime;
    bool           mBuildingPath;
    float          mSearchPatternAngle;
    UMath::Vector3 mSearchDestPoint;
    IRigidBody*    mPlayerRigidBody;
    UMath::Vector3 mPlayerPosition;
    UMath::Vector3 mSkidHitOffset;
    int            mCollisionAbort;
    float          mPlayerSpeed;
    kPursuitMode   mPursuitMode;
};
