// HeliRadioChat.h - restored helicopter police-radio speech.
//
// NFSMW ships a full set of helicopter speech events in copspeech.evt
// (HeliSpecific_*) with recorded audio in copspeech.big, but most of them
// are never triggered by the game. This module plays them through the
// game's own pursuit speech scheduler, so every line gets the normal
// police-radio processing, channel, priority and queueing - no raw audio
// playback of any kind.
//
// Design rules:
//  - Native only: events are triggered through the executable's own
//    per-event trigger methods (or, for the dispatcher ETA line, the same
//    scheduler call the game uses for other dispatcher lines). Every
//    function is byte-guard verified in Initialize() before any call.
//  - Edge-triggered: speech fires on gameplay-state transitions observed
//    by the existing HelicopterOptions systems, never per frame.
//  - Self-limiting: global cooldown, per-event cooldowns, a per-minute
//    budget, and validity checks (pursuit speech active, helicopter
//    tracked, player tracked) keep lines occasional.
//  - Update() runs from the existing game-thread helicopter update hook;
//    no additional hook is installed for speech.
#pragma once
#include "../Game/HeliState.h"

namespace Systems { namespace HeliRadioChat {

    // Verify all native speech functions/tables against the executable.
    // Returns false (and disables the module) if any guard fails.
    bool Initialize();

    // Forget all per-pursuit state (safe to call any time).
    void Reset();

    // Release the module (Reset + stop accepting Update calls).
    void Shutdown();

    // Per-tick update from the helicopter update hook (game thread).
    // The snapshot is the already-validated helicopter state for this tick.
    void Update(const HeliState::Snapshot& s);

    bool Enabled();   // configured on AND all guards verified

} } // namespace Systems::HeliRadioChat
