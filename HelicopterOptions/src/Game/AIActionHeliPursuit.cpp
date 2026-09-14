#include <cstdint>
#include "AIActionHeliPursuit.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/Detour.h"
#include "../Core/Log.h"
#include "../Core/Memory.h"
#include "../Core/PatchManager.h"

namespace AIActionHeliPursuit {

    namespace {

        namespace Game = Addr::AIActionHeliPursuit;

        constexpr float kBlockedEntryMinDistance = 25.0f;
        constexpr float kBlockedEntryMaxDistance = 5.0f;
        constexpr int   kTrackedActions          = 4;

        struct LiveValues {
            float skidCooldown;
            float skidEntryMinDistance;
            float skidEntryMaxDistance;
            float skidEntryAlignment;
            float skidEntryMaxHeight;
            float leadSpeedScale;
            float leadBase;
            float leadMax;
            float chaseHeightSkid;
            float chaseHeightClose;
            float chaseHeightHigh;
        };

        LiveValues gLive = {};
        bool       gAttacksBlocked = false;
        void*      gActions[kTrackedActions] = {};
        int        gNextAction = 0;
        void*      gAction = nullptr;
        bool       gAttacking = false;
        int        gAttackRuns = 0;
        float      gReattackTimer = 0.0f;

        void WriteEntryGates() {
            gLive.skidEntryMinDistance = gAttacksBlocked ? kBlockedEntryMinDistance : gCfg.SkidEntryMinDistance;
            gLive.skidEntryMaxDistance = gAttacksBlocked ? kBlockedEntryMaxDistance : gCfg.SkidEntryMaxDistance;
        }

        void SetAttacksBlocked(bool blocked) {
            if (blocked == gAttacksBlocked) return;
            gAttacksBlocked = blocked;
            WriteEntryGates();
        }

        bool BelongsTo(void* action, void* rigidBody) {
            uint32_t table = 0;
            uint32_t body = 0;
            return action
                && Memory::Read(reinterpret_cast<uintptr_t>(action), &table, sizeof(table))
                && table == Game::Vtable
                && Memory::Read(reinterpret_cast<uintptr_t>(action) + Game::RigidBody, &body, sizeof(body))
                && body == static_cast<uint32_t>(reinterpret_cast<uintptr_t>(rigidBody));
        }

        void __cdecl ConstructorEntry(Detour::Registers* registers) {
            void* action = reinterpret_cast<void*>(static_cast<uintptr_t>(registers->ecx));
            if (!action) return;
            for (void* known : gActions)
                if (known == action) return;
            gActions[gNextAction] = action;
            gNextAction = (gNextAction + 1) % kTrackedActions;
        }

    }

    void Refresh() {
        gLive.skidCooldown       = gCfg.SkidCooldown;
        gLive.skidEntryAlignment = gCfg.SkidEntryAlignment;
        gLive.skidEntryMaxHeight = gCfg.SkidEntryMaxHeight;
        gLive.leadSpeedScale     = gCfg.LeadSpeedScale;
        gLive.leadBase           = gCfg.LeadBase;
        gLive.leadMax            = gCfg.LeadMax;
        gLive.chaseHeightSkid    = gCfg.ChaseHeightSkid;
        gLive.chaseHeightClose   = gCfg.ChaseHeightClose;
        gLive.chaseHeightHigh    = gCfg.ChaseHeightHigh;
        WriteEntryGates();
    }

    void InstallPatches() {
        Refresh();
        Patch::Begin("AIActionHeliPursuit::StraightLinePursuit");
        Patch::RedirectFloat("SkidCooldown", Game::SkidCooldownGate, &gLive.skidCooldown);
        Patch::RedirectFloat("SkidCooldown height mode", Game::SkidCooldownHeightMode, &gLive.skidCooldown);
        Patch::RedirectFloat("SkidEntryMaxDistance", Game::SkidEntryMaxDistance, &gLive.skidEntryMaxDistance);
        Patch::RedirectFloat("SkidEntryMinDistance", Game::SkidEntryMinDistance, &gLive.skidEntryMinDistance);
        Patch::RedirectFloat("SkidEntryAlignment", Game::SkidEntryAlignment, &gLive.skidEntryAlignment);
        Patch::RedirectFloat("SkidEntryMaxHeight", Game::SkidEntryMaxHeight, &gLive.skidEntryMaxHeight);
        Patch::RedirectFloat("LeadSpeedScale", Game::LeadSpeedScale, &gLive.leadSpeedScale);
        Patch::RedirectFloat("LeadBase", Game::LeadBase, &gLive.leadBase);
        Patch::RedirectFloat("LeadMax", Game::LeadMax, &gLive.leadMax);
        Patch::RedirectFloat("ChaseHeightSkid", Game::ChaseHeightSkid, &gLive.chaseHeightSkid);
        Patch::RedirectFloat("ChaseHeightClose", Game::ChaseHeightClose, &gLive.chaseHeightClose);
        Patch::RedirectFloat("ChaseHeightHigh", Game::ChaseHeightHigh, &gLive.chaseHeightHigh);
        Patch::Commit();
    }

    bool HookConstructor() {
        return Detour::Install("AIActionHeliPursuit::AIActionHeliPursuit", Game::Constructor, Game::ConstructorPrologue,
                               sizeof(Game::ConstructorPrologue), &ConstructorEntry);
    }

    int ReadMode(void* rigidBody) {
        if (!BelongsTo(gAction, rigidBody)) {
            gAction = nullptr;
            for (void* candidate : gActions) {
                if (BelongsTo(candidate, rigidBody)) {
                    gAction = candidate;
                    break;
                }
            }
            if (!gAction) return -1;
        }
        uint32_t mode = 0;
        if (!Memory::Read(reinterpret_cast<uintptr_t>(gAction) + Game::Mode, &mode, sizeof(mode)) || mode > 3u)
            return -1;
        return static_cast<int>(mode);
    }

    void BeginHelicopter() {
        gAttacking = false;
        gAttackRuns = 0;
        gReattackTimer = 0.0f;
        SetAttacksBlocked(false);
    }

    void TrackAttacks(int mode, float dt) {
        const bool attacking = mode >= 2;
        if (attacking && !gAttacking)
            Log::Info("Helicopter attack run %d.", ++gAttackRuns);
        if (!attacking && gAttacking)
            gReattackTimer = gCfg.ReattackDelay;
        gAttacking = attacking;

        if (gReattackTimer > 0.0f) gReattackTimer -= dt;
        SetAttacksBlocked(!attacking && gReattackTimer > 0.0f);
    }

}
