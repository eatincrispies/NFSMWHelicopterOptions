#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "AIVehicleHelicopter.h"
#include "AIActionHeliPursuit.h"
#include "AIPerpVehicle.h"
#include "HeliSheet.h"
#include "Interfaces.h"
#include "SimpleChopper.h"
#include "../Config/Config.h"

namespace AIVehicleHelicopter {

    namespace {

        constexpr uintptr_t kHeliVehicle          = 0x0090D61Cu;
        constexpr uintptr_t kOnDriving            = 0x00417A20u;
        constexpr uint8_t   kOnDrivingPrologue[5] = { 0x83, 0xEC, 0x68, 0x53, 0x55 };
        constexpr uintptr_t kLineOfSightStore     = 0x00417146u;
        constexpr uint8_t   kLineOfSightStorePrologue[8] = { 0x89, 0x54, 0x24, 0x24, 0xD9, 0x44, 0x24, 0x24 };
        constexpr unsigned  kOwner                = 0x34u;
        constexpr unsigned  kDriveSpeed           = 0x84u;
        constexpr unsigned  kFuelTimeRemaining    = 0x7D8u;
        constexpr unsigned  kISimpleChopper       = 0x8B0u;

        constexpr float kReferenceStep = 1.0f / 60.0f;

        uint32_t Detour::Registers::* const kRegisters[] = {
            &Detour::Registers::ecx, &Detour::Registers::esi, &Detour::Registers::edi, &Detour::Registers::ebx,
            &Detour::Registers::ebp, &Detour::Registers::eax, &Detour::Registers::edx,
        };
        const char* const kRegisterNames[] = { "ECX", "ESI", "EDI", "EBX", "EBP", "EAX", "EDX" };
        constexpr int kRegisterCount = static_cast<int>(sizeof(kRegisters) / sizeof(kRegisters[0]));

        int           gRegister = -1;
        unsigned long gFailStreak = 0;
        unsigned long gLastProbeWarningMs = 0;
        unsigned long gLastProblemMs = 0;
        unsigned long gLastRotateMs = 0;
        void*         gOwner = nullptr;
        float         gLastFuel = 0.0f;

        LARGE_INTEGER gFrequency = {};
        LARGE_INTEGER gLastTick = {};
        bool          gCounter = false;
        bool          gHaveLastTick = false;
        float         gSmoothed = 0.0f;
        unsigned      gFrames = 0;
        float         gElapsed = 0.0f;
        bool          gReported = false;

        void ResetClock() {
            gHaveLastTick = false;
            gSmoothed = 0.0f;
        }

        void InitClock() {
            gCounter = QueryPerformanceFrequency(&gFrequency) != 0 && gFrequency.QuadPart > 0;
            if (!gCounter)
                Log::Warn("No high-resolution timer is available; the helicopter's motion is smoothed as if at 60 FPS.");
            ResetClock();
        }

        void AdvanceClock() {
            float dt = kReferenceStep;
            LARGE_INTEGER now;
            if (gCounter && QueryPerformanceCounter(&now)) {
                if (gHaveLastTick) {
                    const double seconds = static_cast<double>(now.QuadPart - gLastTick.QuadPart)
                                         / static_cast<double>(gFrequency.QuadPart);
                    if (seconds > 0.0 && seconds < 100.0) dt = static_cast<float>(seconds);
                }
                gLastTick = now;
                gHaveLastTick = true;
            }

            const float outlier = kReferenceStep * 3.0f;
            if (gSmoothed <= 0.0f)
                gSmoothed = dt <= outlier ? dt : kReferenceStep;
            else if (dt <= outlier)
                gSmoothed += (dt - gSmoothed) * std::clamp(dt / 0.25f, 0.0f, 1.0f);
            gSmoothed = std::clamp(gSmoothed, kReferenceStep * 0.125f, kReferenceStep);

            ++gFrames;
            gElapsed += dt;
            if (!gReported && gElapsed >= 2.0f) {
                gReported = true;
                Log::Info("Helicopter updates arrive %.0f times per second.", gFrames / gElapsed);
            }
        }

        float SmoothedFrames() {
            return StepSeconds() / kReferenceStep;
        }

        uintptr_t Field(void* heli, unsigned offset) {
            return reinterpret_cast<uintptr_t>(heli) + offset;
        }

        bool HeliVehicleActive() {
            void* heli = nullptr;
            return Memory::Read(kHeliVehicle, &heli, sizeof(heli)) && heli;
        }

        bool Validate(void* heli, void** ownerOut, void** rigidBodyOut, float position[3]) {
            void* owner = nullptr;
            void* rigidBody = nullptr;
            float driveSpeed = 0.0f;
            if (!heli || !HeliVehicleActive()
                || !Memory::Read(Field(heli, kOwner), &owner, sizeof(owner)) || !owner
                || !Interfaces::ReadRigidBody(owner, &rigidBody)
                || !Interfaces::ReadPosition(rigidBody, position)
                || !Memory::Read(Field(heli, kDriveSpeed), &driveSpeed, sizeof(driveSpeed))
                || !Memory::IsFinite(driveSpeed) || driveSpeed < -1.0e4f || driveSpeed > 1.0e4f)
                return false;

            if (ownerOut) *ownerOut = owner;
            if (rigidBodyOut) *rigidBodyOut = rigidBody;
            return true;
        }

        void* RegisterValue(const Detour::Registers& registers, int index) {
            return reinterpret_cast<void*>(static_cast<uintptr_t>(registers.*kRegisters[index]));
        }

        void* FindHelicopter(const Detour::Registers& registers) {
            if (!HeliVehicleActive()) return nullptr;

            float position[3];
            if (gRegister >= 0) {
                void* heli = RegisterValue(registers, gRegister);
                if (Validate(heli, nullptr, nullptr, position)) {
                    gFailStreak = 0;
                    return heli;
                }
                if (++gFailStreak > 600) {
                    Log::Warn("AIVehicleHelicopter::OnDriving stopped receiving the helicopter in %s; searching the registers again.",
                              kRegisterNames[gRegister]);
                    gRegister = -1;
                    gFailStreak = 0;
                }
                return nullptr;
            }

            for (int i = 0; i < kRegisterCount; ++i) {
                void* heli = RegisterValue(registers, i);
                if (Validate(heli, nullptr, nullptr, position)) {
                    gRegister = i;
                    Log::Info("AIVehicleHelicopter::OnDriving receives the helicopter in %s.", kRegisterNames[i]);
                    return heli;
                }
            }

            const unsigned long now = GetTickCount();
            if (now - gLastProbeWarningMs >= 5000) {
                gLastProbeWarningMs = now;
                Log::Warn("AIVehicleHelicopter::OnDriving: no register held a valid helicopter.");
            }
            return nullptr;
        }

        void BeginHelicopter(const Snapshot& s) {
            gOwner = s.owner;
            HeliSheet::BeginHelicopter();
            AIActionHeliPursuit::ResetLead();
            ResetClock();

            float playerPosition[3];
            if (Interfaces::ReadPlayerPosition(playerPosition)) {
                const float dx = s.position[0] - playerPosition[0];
                const float dz = s.position[2] - playerPosition[2];
                Log::Info("Helicopter spawned %.0f m from you and %.0f m above you, with %.0f s of fuel.",
                          std::sqrt(dx * dx + dz * dz), s.position[1] - playerPosition[1], s.fuel);
            } else {
                Log::Info("Helicopter spawned with %.0f s of fuel.", s.fuel);
            }

            gLastFuel = s.fuel;
            if (gCfg.FuelTime > 0.0f && WriteFuelTime(s, gCfg.FuelTime)) {
                gLastFuel = gCfg.FuelTime;
                Log::Info("Fuel set to %.0f s by [Helicopter:FuelTime].", gCfg.FuelTime);
            }
            if (gCfg.LineOfSight > 0.0f)
                Log::Info("Line of sight set to %.0f m by [Helicopter:LineOfSight].", gCfg.LineOfSight);
        }

        void __cdecl LineOfSightEntry(Detour::Registers* registers) {
            if (gCfg.LineOfSight <= 0.0f) return;
            std::memcpy(&registers->edx, &gCfg.LineOfSight, sizeof(registers->edx));
        }

        void OnDriving(void* heli) {
            Snapshot s;
            if (!Capture(heli, &s)) return;

            const unsigned long now = GetTickCount();
            if (now - gLastRotateMs >= 30000) {
                gLastRotateMs = now;
                Log::CheckRotate();
            }

            AIPerpVehicle::UpdateHeat();

            const bool newHelicopter = s.owner != gOwner || s.fuel > gLastFuel + 1.0f;
            if (newHelicopter)
                BeginHelicopter(s);
            else
                gLastFuel = s.fuel;

            HeliSheet::Update(s);
            SimpleChopper::ApplySpeedCap(s);
            if (newHelicopter) return;

            SimpleChopper::ScaleMotionFilters(SmoothedFrames());
        }

        void __cdecl OnDrivingEntry(Detour::Registers* registers) {
            void* heli = FindHelicopter(*registers);
            if (!heli) return;
            AdvanceClock();
            OnDriving(heli);
        }

    }

    bool Capture(void* heli, Snapshot* out) {
        std::memset(out, 0, sizeof(*out));
        if (!Validate(heli, &out->owner, &out->rigidBody, out->position)) return false;
        out->heli = heli;
        Memory::Read(Field(heli, kISimpleChopper), &out->chopper, sizeof(out->chopper));

        if (!Memory::Read(Field(heli, kFuelTimeRemaining), &out->fuel, sizeof(out->fuel)) || !Memory::IsFinite(out->fuel)) {
            const unsigned long now = GetTickCount();
            if (now - gLastProblemMs >= 30000) {
                gLastProblemMs = now;
                Log::Warn("The helicopter's fuel could not be read this frame.");
            }
            return false;
        }

        out->mode = AIActionHeliPursuit::ReadMode(out->rigidBody);
        return true;
    }

    bool WriteFuelTime(const Snapshot& snapshot, float seconds) {
        return snapshot.heli && Memory::IsFinite(seconds) && seconds >= 0.0f
            && Memory::WriteData(Field(snapshot.heli, kFuelTimeRemaining), &seconds, sizeof(seconds));
    }

    bool HookOnDriving() {
        InitClock();
        return Detour::Install("AIVehicleHelicopter::OnDriving", kOnDriving, kOnDrivingPrologue,
                               sizeof(kOnDrivingPrologue), &OnDrivingEntry);
    }

    bool HookLineOfSight() {
        return Detour::Install("AIVehicleHelicopter line-of-sight check", kLineOfSightStore, kLineOfSightStorePrologue,
                               sizeof(kLineOfSightStorePrologue), &LineOfSightEntry);
    }

    float StepSeconds() {
        return gSmoothed > 0.0f ? gSmoothed : kReferenceStep;
    }

}
