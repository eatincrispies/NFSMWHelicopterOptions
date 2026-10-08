#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "AIActionHeliPursuit.h"
#include "AIVehicleHelicopter.h"
#include "Interfaces.h"
#include "../Config/Config.h"

namespace AIActionHeliPursuit {

    namespace {

        constexpr uintptr_t kConstructor            = 0x00420EC0u;
        constexpr uint8_t   kConstructorPrologue[7] = { 0x6A, 0xFF, 0x68, 0xF8, 0x7D, 0x86, 0x00 };
        constexpr uintptr_t kVtable                 = 0x00891358u;
        constexpr unsigned  kRigidBody              = 0x54u;
        constexpr unsigned  kMode                   = 0xA4u;

        constexpr Patch::FloatOperand kLeadBase             = { 0x00412946u, { 0xD8, 0x05 }, 0x00890614u };
        constexpr Patch::FloatOperand kLeadMax              = { 0x0041294Cu, { 0xD8, 0x15 }, 0x00890658u };
        constexpr Patch::CallSite     kLeadHeadingNormalize = { 0x00412932u, 0x0040FE20u };

        constexpr int           kTrackedActions  = 4;
        constexpr unsigned long kLeadSnapAfterMs = 1000;
        constexpr float         kSmallestLength  = 1.0e-3f;

        using NormalizeCall = void (__cdecl*)(const float* in, float* out);

        struct LiveValues {
            float leadBase;
            float leadMax;
        };

        LiveValues    gLive = {};
        void*         gActions[kTrackedActions] = {};
        int           gNextAction = 0;
        void*         gAction = nullptr;
        float         gLeadHeading[3] = {};
        bool          gHaveLeadHeading = false;
        unsigned long gLastLeadMs = 0;

        bool BelongsTo(void* action, void* rigidBody) {
            uint32_t table = 0;
            uint32_t body = 0;
            return action
                && Memory::Read(reinterpret_cast<uintptr_t>(action), &table, sizeof(table))
                && table == kVtable
                && Memory::Read(reinterpret_cast<uintptr_t>(action) + kRigidBody, &body, sizeof(body))
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

        bool IsFinite3(const float* vector) {
            return Memory::IsFinite(vector[0]) && Memory::IsFinite(vector[1]) && Memory::IsFinite(vector[2]);
        }

        void SmoothLeadHeading(float* heading) {
            const float seconds = gCfg.LeadSmoothing;
            if (seconds <= 0.0f || !IsFinite3(heading)) {
                gHaveLeadHeading = false;
                return;
            }

            const unsigned long now = GetTickCount();
            const bool stale = now - gLastLeadMs > kLeadSnapAfterMs;
            gLastLeadMs = now;
            if (!gHaveLeadHeading || stale) {
                std::memcpy(gLeadHeading, heading, sizeof(gLeadHeading));
                gHaveLeadHeading = true;
                return;
            }

            const float weight = 1.0f - std::exp(-AIVehicleHelicopter::StepSeconds() / seconds);
            float length = 0.0f;
            for (int i = 0; i < 3; ++i) {
                gLeadHeading[i] += (heading[i] - gLeadHeading[i]) * weight;
                length += gLeadHeading[i] * gLeadHeading[i];
            }
            length = std::sqrt(length);
            if (length < kSmallestLength) {
                std::memcpy(gLeadHeading, heading, sizeof(gLeadHeading));
                return;
            }
            for (int i = 0; i < 3; ++i) {
                gLeadHeading[i] /= length;
                heading[i] = gLeadHeading[i];
            }
        }

        void __cdecl LeadHeadingHook(const float* in, float* out) {
            reinterpret_cast<NormalizeCall>(kLeadHeadingNormalize.target)(in, out);
            SmoothLeadHeading(out);
        }

    }

    void Refresh() {
        gLive.leadBase = gCfg.LeadBase;
        gLive.leadMax  = gCfg.LeadMax;
    }

    void ResetLead() {
        gHaveLeadHeading = false;
    }

    void InstallPatches() {
        Refresh();
        Patch::Begin("AIActionHeliPursuit::StraightLinePursuit");
        Patch::RedirectFloat("LeadBase", kLeadBase, &gLive.leadBase);
        Patch::RedirectFloat("LeadMax", kLeadMax, &gLive.leadMax);
        Patch::RedirectCall("lead heading", kLeadHeadingNormalize, reinterpret_cast<const void*>(&LeadHeadingHook));
        Patch::Commit();
    }

    bool HookConstructor() {
        return Detour::Install("AIActionHeliPursuit::AIActionHeliPursuit", kConstructor, kConstructorPrologue,
                               sizeof(kConstructorPrologue), &ConstructorEntry);
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
        if (!Memory::Read(reinterpret_cast<uintptr_t>(gAction) + kMode, &mode, sizeof(mode)) || mode > 3u)
            return -1;
        return static_cast<int>(mode);
    }

}
