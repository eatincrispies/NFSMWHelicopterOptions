#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <cstddef>
#include "HookManager.h"
#include "Addresses.h"
#include "Memory.h"
#include "Log.h"
#include "FrameTime.h"
#include "../Game/HeliState.h"

namespace Hook {

    namespace {
        constexpr int kMaxTicks = 8;
        TickFn gTicks[kMaxTicks] = {};
        int    gTickCount = 0;
        TickFn gCtorFn = nullptr;
        TickFn gActionCtorFn = nullptr;

        struct Installed {
            uintptr_t va;
            uint8_t   original[16];
            size_t    stolenLen;
            bool      active;
        };
        Installed gOnDriving = {};
        Installed gCtor = {};
        Installed gActionCtor = {};

        // Register frame as laid out by "pushfd; pushad" (ESP after pushad).
        struct RegFrame {
            uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;   // pushad order
            uint32_t eflags;                                    // pushed first
        };

        struct Candidate { const char* name; unsigned offset; };
        const Candidate kCandidates[] = {
            { "ECX", offsetof(RegFrame, ecx) },
            { "ESI", offsetof(RegFrame, esi) },
            { "EDI", offsetof(RegFrame, edi) },
            { "EBX", offsetof(RegFrame, ebx) },
            { "EBP", offsetof(RegFrame, ebp) },
            { "EAX", offsetof(RegFrame, eax) },
            { "EDX", offsetof(RegFrame, edx) },
        };
        constexpr int kNumCandidates = sizeof(kCandidates) / sizeof(kCandidates[0]);

        int           gLockedReg = -1;
        unsigned long gLastProbeFailMs = 0;
        unsigned long gLockedFailStreak = 0;

        uint32_t RegValue(const RegFrame* f, int idx) {
            return *reinterpret_cast<const uint32_t*>(
                reinterpret_cast<const uint8_t*>(f) + kCandidates[idx].offset);
        }

        void* ResolveHeliThis(const RegFrame* f) {
            void* globalHeli = nullptr;
            if (!Memory::ReadPtr(Addr::kGlobalHeliVehicle, &globalHeli) || !globalHeli)
                return nullptr;   // no helicopter exists (normal between pursuits)

            HeliState::RejectReason why;
            void* owner = nullptr; void* rb = nullptr;

            if (gLockedReg >= 0) {
                void* p = reinterpret_cast<void*>(RegValue(f, gLockedReg));
                if (HeliState::ValidateStructural(p, &owner, &rb, &why)) {
                    gLockedFailStreak = 0;
                    return p;
                }
                if (++gLockedFailStreak > 600) {
                    Log::Warn("[Hook] locked register %s failed validation for %lu "
                              "consecutive ticks - re-probing all registers.",
                              kCandidates[gLockedReg].name, gLockedFailStreak);
                    gLockedReg = -1;
                    gLockedFailStreak = 0;
                }
                return nullptr;
            }

            for (int i = 0; i < kNumCandidates; ++i) {
                void* p = reinterpret_cast<void*>(RegValue(f, i));
                if (HeliState::ValidateStructural(p, &owner, &rb, &why)) {
                    gLockedReg = i;
                    Log::Info("[Hook] OnDriving 'this' located in %s: aiThis=%p owner=%p "
                              "rigidBody=%p gHeliVehicle=%p (register locked).",
                              kCandidates[i].name, p, owner, rb, globalHeli);
                    return p;
                }
            }
            const unsigned long now = GetTickCount();
            if (now - gLastProbeFailMs >= 5000) {
                gLastProbeFailMs = now;
                Log::Warn("[Hook] no register at the OnDriving prologue passed structural "
                          "helicopter validation this probe (gHeliVehicle=%p, "
                          "ECX=%08lX ESI=%08lX EDI=%08lX EBX=%08lX EBP=%08lX).",
                          globalHeli,
                          static_cast<unsigned long>(f->ecx), static_cast<unsigned long>(f->esi),
                          static_cast<unsigned long>(f->edi), static_cast<unsigned long>(f->ebx),
                          static_cast<unsigned long>(f->ebp));
            }
            return nullptr;
        }

        uint8_t* BuildTrampoline(uintptr_t va, const uint8_t* stolen, size_t stolenLen,
                                 void* dispatch) {
            uint8_t* tramp = static_cast<uint8_t*>(
                VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            if (!tramp) return nullptr;
            uint8_t* p = tramp;
            *p++ = 0x9C;                                   // pushfd
            *p++ = 0x60;                                   // pushad
            *p++ = 0xFC;                                   // cld
            *p++ = 0x54;                                   // push esp (RegFrame*)
            *p++ = 0xE8;                                   // call rel32
            {
                const intptr_t rel = reinterpret_cast<intptr_t>(dispatch)
                                   - reinterpret_cast<intptr_t>(p + 4);
                std::memcpy(p, &rel, 4); p += 4;
            }
            *p++ = 0x83; *p++ = 0xC4; *p++ = 0x04;         // add esp,4
            *p++ = 0x61;                                   // popad
            *p++ = 0x9D;                                   // popfd
            std::memcpy(p, stolen, stolenLen); p += stolenLen;
            *p++ = 0xE9;                                   // jmp rel32
            {
                const intptr_t rel = static_cast<intptr_t>(va + stolenLen)
                                   - reinterpret_cast<intptr_t>(p + 4);
                std::memcpy(p, &rel, 4); p += 4;
            }
            FlushInstructionCache(GetCurrentProcess(), tramp, 64);
            return tramp;
        }

        bool InstallAt(const char* name, Installed& slot, uintptr_t va,
                       const uint8_t* guard, size_t stolenLen, void* dispatch) {
            if (slot.active) return true;
            if (!Memory::CheckBytes(va, guard, stolenLen)) {
                uint8_t actual[16]{};
                char actHex[40]{};
                if (Memory::ReadBytes(va, actual, stolenLen))
                    Log::HexString(actual, static_cast<unsigned>(stolenLen), actHex, sizeof(actHex));
                Log::Warn("%s hook skipped: prologue guard failed at 0x%08lX (actual %s). "
                          "Another mod may hook it.", name,
                          static_cast<unsigned long>(va), actHex);
                return false;
            }
            uint8_t* tramp = BuildTrampoline(va, guard, stolenLen, dispatch);
            if (!tramp) { Log::Error("%s hook: trampoline allocation failed.", name); return false; }

            std::memcpy(slot.original, guard, stolenLen);
            slot.va = va;
            slot.stolenLen = stolenLen;

            uint8_t patch[16];
            patch[0] = 0xE9;
            const intptr_t rel = reinterpret_cast<intptr_t>(tramp)
                               - static_cast<intptr_t>(va + 5);
            std::memcpy(&patch[1], &rel, 4);
            for (size_t i = 5; i < stolenLen; ++i) patch[i] = 0x90;
            if (!Memory::WriteBytes(va, patch, stolenLen)) {
                Log::Error("%s hook: could not write detour.", name);
                return false;
            }
            slot.active = true;
            Log::Info("%s hook installed at 0x%08lX (trampoline %p).",
                      name, static_cast<unsigned long>(va), tramp);
            return true;
        }

    } // namespace

    extern "C" void __cdecl HO_TickDispatch(void* frame) {
        void* heli = ResolveHeliThis(static_cast<const RegFrame*>(frame));
        if (!heli) return;
        // One frame clock advance per frame, before any consumer runs, so
        // every tick consumer sees the same timestep.
        FrameTime::BeginFrame();
        for (int i = 0; i < gTickCount; ++i)
            if (gTicks[i]) gTicks[i](heli);
    }

    extern "C" void __cdecl HO_ActionCtorDispatch(void* frame) {
        // AIActionHeliPursuit ctor (__thiscall, this in ECX). Identity only:
        // the object is provisional until its fields validate at read time.
        if (gActionCtorFn) gActionCtorFn(reinterpret_cast<void*>(
            static_cast<const RegFrame*>(frame)->ecx));
    }

    extern "C" void __cdecl HO_CtorDispatch(void* frame) {
        // Constructor convention (__thiscall, this in ECX) matches the ctor
        // decompile; the value is used as identity only.
        if (gCtorFn) gCtorFn(reinterpret_cast<void*>(
            static_cast<const RegFrame*>(frame)->ecx));
    }

    bool RegisterTick(TickFn fn) {
        if (gTickCount >= kMaxTicks || !fn) return false;
        gTicks[gTickCount++] = fn;
        return true;
    }

    bool InstallOnDrivingHook() {
        if (gTickCount == 0) return false;
        return InstallAt("OnDriving", gOnDriving, Addr::kOnDrivingHookVA,
                         Addr::Guard::OnDrivingPrologue, 5,
                         reinterpret_cast<void*>(&HO_TickDispatch));
    }

    bool InstallHeliActionCtorHook(TickFn fn) {
        gActionCtorFn = fn;
        return InstallAt("HeliActionCtor", gActionCtor, Addr::kHeliActionCtor,
                         Addr::Guard::HeliActionCtorPrologue, 7,
                         reinterpret_cast<void*>(&HO_ActionCtorDispatch));
    }

    bool InstallHeliCtorHook(TickFn fn) {
        gCtorFn = fn;
        return InstallAt("HeliCtor", gCtor, Addr::kHeliCtor,
                         Addr::Guard::HeliCtorPrologue, 7,
                         reinterpret_cast<void*>(&HO_CtorDispatch));
    }

    void Remove() {
        Installed* slots[3] = { &gOnDriving, &gCtor, &gActionCtor };
        const char* names[3] = { "OnDriving", "HeliCtor", "HeliActionCtor" };
        for (int i = 0; i < 3; ++i) {
            if (!slots[i]->active) continue;
            if (Memory::WriteBytes(slots[i]->va, slots[i]->original, slots[i]->stolenLen))
                Log::Info("%s hook removed (original prologue restored).", names[i]);
            else
                Log::Warn("%s hook: failed to restore prologue.", names[i]);
            slots[i]->active = false;
        }
    }

} // namespace Hook
