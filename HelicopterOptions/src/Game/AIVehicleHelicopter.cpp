#define WIN32_LEAN_AND_MEAN
#include <windows.h>
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
#include "../Core/Addresses.h"
#include "../Core/Detour.h"
#include "../Core/FrameTime.h"
#include "../Core/Log.h"
#include "../Core/Memory.h"

namespace AIVehicleHelicopter {

    namespace {

        namespace Game = Addr::AIVehicleHelicopter;

        uint32_t Detour::Registers::* const kRegisters[] = {
            &Detour::Registers::ecx, &Detour::Registers::esi, &Detour::Registers::edi, &Detour::Registers::ebx,
            &Detour::Registers::ebp, &Detour::Registers::eax, &Detour::Registers::edx,
        };
        const char* const kRegisterNames[] = { "ECX", "ESI", "EDI", "EBX", "EBP", "EAX", "EDX" };
        constexpr int kRegisterCount = static_cast<int>(sizeof(kRegisters) / sizeof(kRegisters[0]));

        int           gRegister = -1;
        unsigned long gFailStreak = 0;
        unsigned long gLastProbeWarningMs = 0;
        void*         gOwner = nullptr;
        float         gLastFuel = 0.0f;
        unsigned      gVerticalLimits = 0;
        unsigned long gLastRotateMs = 0;
        int           gLastProblem = 0;
        unsigned long gLastProblemMs = 0;

        uintptr_t Field(void* heli, unsigned offset) {
            return reinterpret_cast<uintptr_t>(heli) + offset;
        }

        bool HeliVehicleActive() {
            void* heli = nullptr;
            return Memory::Read(Addr::gHeliVehicle, &heli, sizeof(heli)) && heli;
        }

        void ReportProblem(int problem, const char* what) {
            const unsigned long now = GetTickCount();
            if (problem == gLastProblem && now - gLastProblemMs < 30000) return;
            gLastProblem = problem;
            gLastProblemMs = now;
            Log::Warn("The helicopter's %s could not be read this frame.", what);
        }

        bool Validate(void* heli, void** ownerOut, void** rigidBodyOut) {
            void* owner = nullptr;
            void* rigidBody = nullptr;
            float position[3];
            float velocity[3];
            float driveSpeed = 0.0f;
            if (!heli || !HeliVehicleActive()
                || !Memory::Read(Field(heli, Game::Owner), &owner, sizeof(owner)) || !owner
                || !Interfaces::CallGetter(owner, Addr::ISimable::GetRigidBody, &rigidBody)
                || !Interfaces::ReadRigidBody(rigidBody, position, velocity, nullptr)
                || !Memory::Read(Field(heli, Game::DriveSpeed), &driveSpeed, sizeof(driveSpeed))
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

            if (gRegister >= 0) {
                void* heli = RegisterValue(registers, gRegister);
                if (Validate(heli, nullptr, nullptr)) {
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
                if (Validate(heli, nullptr, nullptr)) {
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
            AIActionHeliPursuit::BeginHelicopter();
            HeliSheet::BeginHelicopter();
            SimpleChopper::BeginHelicopter();
            FrameTime::Reset();

            float playerPosition[3];
            float playerVelocity[3];
            if (Interfaces::ReadPlayer(playerPosition, playerVelocity)) {
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
                Log::Info("Fuel set to %.0f s by [AIVehicleHelicopter:FuelTime].", gCfg.FuelTime);
            }
        }

        void LimitVerticalSpeed(const Snapshot& s) {
            const float limit = gCfg.MaxVerticalSpeed;
            const float vertical = s.velocity[1];
            if (limit <= 0.0f || (vertical <= limit && vertical >= -limit)) return;

            const float capped = vertical > limit ? limit : -limit;
            if (WriteVerticalVelocity(s, capped) && gVerticalLimits++ % 30 == 0)
                Log::Info("Vertical speed %.1f m/s limited to %.1f m/s (%u time(s) so far).", vertical, capped, gVerticalLimits);
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

            SimpleChopper::ScaleMotionFilters(FrameTime::SmoothedDelta());

            const float dt = FrameTime::Step();
            if (dt <= 0.0f) return;

            AIActionHeliPursuit::TrackAttacks(s.mode, dt);
            LimitVerticalSpeed(s);
        }

        void __cdecl OnDrivingEntry(Detour::Registers* registers) {
            void* heli = FindHelicopter(*registers);
            if (!heli) return;
            FrameTime::BeginFrame();
            OnDriving(heli);
        }

    }

    bool Capture(void* heli, Snapshot* out) {
        std::memset(out, 0, sizeof(*out));
        if (!Validate(heli, &out->owner, &out->rigidBody)) return false;
        out->heli = heli;

        if (!Interfaces::ReadRigidBody(out->rigidBody, out->position, out->velocity, &out->velocityPointer)) {
            ReportProblem(1, "position and velocity");
            return false;
        }

        if (!Memory::Read(Field(heli, Game::DriveSpeed), &out->driveSpeed, sizeof(float))
            || !Memory::Read(Field(heli, Game::FuelTimeRemaining), &out->fuel, sizeof(float))
            || !Memory::IsFinite(out->fuel)) {
            ReportProblem(2, "drive speed and fuel");
            return false;
        }

        out->mode = AIActionHeliPursuit::ReadMode(out->rigidBody);
        return true;
    }

    bool WriteDriveSpeed(const Snapshot& snapshot, float speed) {
        return snapshot.heli && Memory::IsFinite(speed)
            && Memory::WriteData(Field(snapshot.heli, Game::DriveSpeed), &speed, sizeof(speed));
    }

    bool WriteVerticalVelocity(const Snapshot& snapshot, float y) {
        return snapshot.velocityPointer && Memory::IsFinite(y)
            && Memory::WriteData(reinterpret_cast<uintptr_t>(&snapshot.velocityPointer[1]), &y, sizeof(y));
    }

    bool WriteFuelTime(const Snapshot& snapshot, float seconds) {
        return snapshot.heli && Memory::IsFinite(seconds) && seconds >= 0.0f
            && Memory::WriteData(Field(snapshot.heli, Game::FuelTimeRemaining), &seconds, sizeof(seconds));
    }

    bool HookOnDriving() {
        return Detour::Install("AIVehicleHelicopter::OnDriving", Game::OnDriving, Game::OnDrivingPrologue,
                               sizeof(Game::OnDrivingPrologue), &OnDrivingEntry);
    }

}
