#include <cstddef>
#include <cstdint>
#include "SoundAI.hpp"
#include "Interfaces.hpp"

namespace {

    constexpr uintptr_t kSingletonSoundAI = 0x00993CC8u;
    constexpr unsigned  kEAXCopIntentToRam = 0xF8u;

    static_assert(offsetof(SoundAI, mDispatch) == 0xD8, "SoundAI::mDispatch");
    static_assert(offsetof(SoundAI, mLeader) == 0xDC, "SoundAI::mLeader");
    static_assert(offsetof(SoundAI, mHeli) == 0xE0, "SoundAI::mHeli");

}

SoundAI* SoundAI::Get() {
    return Game::Global<SoundAI*>(kSingletonSoundAI);
}

void EAXAirSupport::IntentToRam() {
    Game::ThisCall<void>(Game::VirtualFunction(this, kEAXCopIntentToRam), this);
}
