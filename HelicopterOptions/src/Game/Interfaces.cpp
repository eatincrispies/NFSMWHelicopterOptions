#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "Interfaces.h"
#include "../Core/Addresses.h"
#include "../Core/Memory.h"

namespace Interfaces {

    namespace {

        using Getter = void* (__fastcall*)(void* self, void* unused);

        int SafeCall(void* object, unsigned slot, void** result) {
            __try {
                void** table = *static_cast<void***>(object);
                if (!table || !table[slot / sizeof(void*)]) return 0;
                *result = reinterpret_cast<Getter>(table[slot / sizeof(void*)])(object, nullptr);
                return *result != nullptr;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

    }

    bool CallGetter(void* object, unsigned slot, void** result) {
        return object && SafeCall(object, slot, result) != 0;
    }

    bool ReadRigidBody(void* rigidBody, float position[3], float velocity[3], float** velocityPointer) {
        void* positionData = nullptr;
        void* velocityData = nullptr;
        if (!CallGetter(rigidBody, Addr::IRigidBody::GetPosition, &positionData)
            || !CallGetter(rigidBody, Addr::IRigidBody::GetLinearVelocity, &velocityData)
            || !Memory::Read(reinterpret_cast<uintptr_t>(positionData), position, 3 * sizeof(float))
            || !Memory::Read(reinterpret_cast<uintptr_t>(velocityData), velocity, 3 * sizeof(float)))
            return false;

        for (int i = 0; i < 3; ++i) {
            if (!Memory::IsFinite(position[i]) || position[i] < -1.0e6f || position[i] > 1.0e6f) return false;
            if (!Memory::IsFinite(velocity[i]) || velocity[i] < -1.0e5f || velocity[i] > 1.0e5f) return false;
        }
        if (velocityPointer) *velocityPointer = static_cast<float*>(velocityData);
        return true;
    }

    bool ReadPlayer(float position[3], float velocity[3]) {
        int count = 0;
        void* head = nullptr;
        void* player = nullptr;
        void* simable = nullptr;
        void* rigidBody = nullptr;
        return Memory::Read(Addr::IPlayer::LocalListCount, &count, sizeof(count)) && count != 0
            && Memory::Read(Addr::IPlayer::LocalListHead, &head, sizeof(head)) && head
            && Memory::Read(reinterpret_cast<uintptr_t>(head), &player, sizeof(player)) && player
            && CallGetter(player, Addr::IPlayer::GetSimable, &simable)
            && CallGetter(simable, Addr::ISimable::GetRigidBody, &rigidBody)
            && ReadRigidBody(rigidBody, position, velocity, nullptr);
    }

}
