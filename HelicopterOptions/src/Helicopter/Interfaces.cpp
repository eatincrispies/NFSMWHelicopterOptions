#include <cmath>
#include <cstdint>
#include "Interfaces.hpp"
#include "Hooks.hpp"

namespace {

    constexpr uintptr_t kIPlayerLists = 0x0092D848u;

    struct IPlayerList {
        void*     mVTable;
        IPlayer** mBegin;
        size_t    mCapacity;
        size_t    mSize;
        IPlayer*  mStorage[8];
    };

    static_assert(sizeof(IPlayerList) == 0x30, "ListableSet<IPlayer, 8, ePlayerList, 3>::List is 0x30 bytes");

}

IPlayer* IPlayer::First(ePlayerList idx) {
    const IPlayerList& list = Game::Global<IPlayerList[PLAYER_MAX]>(kIPlayerLists)[idx];
    return list.mSize != 0 ? list.mBegin[0] : nullptr;
}

IRigidBody* GetLocalPlayerRigidBody() {
    IPlayer* player = IPlayer::First(PLAYER_LOCAL);
    ISimable* simable = player != nullptr ? player->GetSimable() : nullptr;
    return simable != nullptr ? simable->GetRigidBody() : nullptr;
}

float UMath::Distancexz(const Vector3& a, const Vector3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dz * dz);
}
