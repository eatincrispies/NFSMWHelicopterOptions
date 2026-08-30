// HeliRadioChat.cpp - restored helicopter police-radio speech.
//
// How playback works (all exe-verified, see Addr::Speech):
//   The game resolves speech events by name through a static table of
//   { const char* name, u32 tag } pairs and schedules them with
//   ScheduleSpeechEvent(argBytes, argBlob, entry, state, source) at
//   0x00713B20. Every HeliSpecific_* event has its own concrete trigger
//   method in the executable (0x00717B10..0x00717D40) that builds the
//   argument blob from the speaker object and calls the scheduler. This
//   module calls exactly those methods, on a speaker obtained from the
//   game's own live Speech system ([0x00993CC8] + GetSpeaker 0x00715610),
//   so lines use the normal radio effect, channel, priority and queue.
//
//   The one event without a trigger method, Backup_DispHeliBUETA (the
//   dispatcher announcing the helicopter), is scheduled with the identical
//   argument pattern the executable uses for other dispatcher lines
//   (0x00717AE0: argBytes=4, blob={speakerId}, source=dispatcher speaker).
//
// Every native address is byte-guard verified in Initialize() before any
// call; a single failed guard disables the whole module. All calls are
// SEH-protected and happen on the game thread from the existing helicopter
// update hook.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "HeliRadioChat.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/Memory.h"
#include "../Core/Log.h"
#include "../Systems/AiCore.h"

namespace Systems { namespace HeliRadioChat {

    namespace {

        // ------------------------------------------------------------ events
        enum Event {
            EV_HELI_INBOUND,      // Backup_DispHeliBUETA (dispatcher)
            EV_ARRIVAL,           // HeliSelfStrategy (follow begins)
            EV_SPOTTER,           // HeliSpotter (acquired / reacquired)
            EV_LOST_VISUAL,       // HeliLostVisual
            EV_QUADRENT,          // HeliQuadrent (stopped-player position)
            EV_QUADRENT_MOVING,   // HeliQuadrentMoving (pursuit resumes)
            EV_INTENT_TO_BAIL,    // HeliIntentToBail (fuel warning)
            EV_BAILOUT,           // HeliBailout (fuel exhausted)
            EV_BULLHORN,          // HeliBullhornArrest
            EV_ATTACK,            // HeliSelfStrategy (attack announced)
            EV_HAZARD,            // HeliHazardAlert
            EV_SWARM,             // HeliSwarming
            EV_COUNT
        };
        const char* kEventNames[EV_COUNT] = {
            "HeliInbound(DispHeliBUETA)", "HeliSelfStrategy", "HeliSpotter",
            "HeliLostVisual", "HeliQuadrent", "HeliQuadrentMoving",
            "HeliIntentToBail", "HeliBailout", "HeliBullhornArrest",
            "HeliSelfStrategy(attack)", "HeliHazardAlert", "HeliSwarming"
        };

        // ------------------------------------------------------------ native
        typedef void  (__fastcall* FnNoArg)(void* self, void* edx);
        typedef void  (__fastcall* FnIntArg)(void* self, void* edx, int a);
        typedef void* (__fastcall* FnGetSpeaker)(void* self, void* edx, int role);
        typedef int   (__cdecl*   FnSchedule)(int argBytes, const void* blob,
                                              const void* entry, void* state,
                                              void* source);

        // SEH wrappers: plain functions, no C++ objects, game thread only.
        bool SafeNoArg(uintptr_t fn, void* self) {
            __try { reinterpret_cast<FnNoArg>(fn)(self, nullptr); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }
        bool SafeIntArg(uintptr_t fn, void* self, int a) {
            __try { reinterpret_cast<FnIntArg>(fn)(self, nullptr, a); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }
        void* SafeGetSpeaker(uintptr_t fn, void* self, int role) {
            __try { return reinterpret_cast<FnGetSpeaker>(fn)(self, nullptr, role); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        }
        bool SafeScheduleId(uintptr_t fn, int id, uintptr_t entry, void* state,
                            void* source) {
            int blob[1] = { id };
            __try {
                reinterpret_cast<FnSchedule>(fn)(4, blob,
                    reinterpret_cast<const void*>(entry), state, source);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        // ------------------------------------------------------------ state
        bool gVerified = false;   // all byte guards passed
        bool gShutdown = false;

        struct PursuitState {
            void*  ownerKey = nullptr;
            unsigned long lastMs = 0;
            float  aliveTime = 0.0f;

            float  globalCooldown = 0.0f;
            float  eventCooldown[EV_COUNT] = {};
            float  minuteWindow = 0.0f;
            int    linesThisMinute = 0;

            bool   inboundSaid = false;
            bool   arrivalSaid = false;
            bool   spotterSaid = false;

            float  farTime = 0.0f;
            bool   lostEpisode = false;
            float  hiddenTime = 0.0f;    // sustained game-reported search time
            float  visibleTime = 0.0f;   // sustained game-reported visual time

            bool   playerStopped = false;
            float  slowTime = 0.0f;
            int    fastSamples = 0;
            float  stoppedTime = 0.0f;
            float  arrestCloseTime = 0.0f;
            bool   quadrentSaid = false;
            bool   bullhornSaid = false;

            bool   fuelWarnSaid = false;
            bool   bailoutSaid = false;
            float  fuelPrev = 1.0e9f;

            bool   attackPrev = false;
            bool   stuckPrev = false;
            bool   swarmSaid = false;

            unsigned suppressed = 0;
            unsigned long lastSuppressLogMs = 0;
            unsigned long lastNoSpeechLogMs = 0;
        };
        PursuitState R;

        float Clampf(float v, float lo, float hi) {
            return v < lo ? lo : (v > hi ? hi : v);
        }

        // ---------------------------------------------------------- speakers
        bool ValidSpeaker(void* spk) {
            if (!spk) return false;
            void* vt = nullptr;
            if (!Memory::ReadPtr(reinterpret_cast<uintptr_t>(spk), &vt) || !vt)
                return false;
            const uintptr_t v = reinterpret_cast<uintptr_t>(vt);
            if (v < Addr::kImageBase || v > Addr::kImageBase + Addr::kSizeOfImage)
                return false;
            uint8_t idBytes[4];
            return Memory::ReadBytes(reinterpret_cast<uintptr_t>(spk)
                                     + Addr::Speech::kSpeakerIdOfs, idBytes, 4);
        }

        void* SpeechSystem() {
            void* sp = nullptr;
            if (!Memory::ReadPtr(Addr::Speech::kSystemPtr, &sp)) return nullptr;
            return sp;
        }

        void* HeliSpeaker(void* sp) {
            // Preferred role first, then the remaining roles: the roles
            // partition the registered pursuit speakers (0 = any).
            const int pref = gCfg.RadioHeliSpeakerRole;
            const int order[3] = { pref, pref == 2 ? 1 : 2, 0 };
            for (int i = 0; i < 3; ++i) {
                void* spk = SafeGetSpeaker(Addr::Speech::kGetSpeaker, sp, order[i]);
                if (ValidSpeaker(spk)) return spk;
            }
            return nullptr;
        }

        void* DispatcherSpeaker(void* sp) {
            void* spk = nullptr;
            if (!Memory::ReadPtr(reinterpret_cast<uintptr_t>(sp)
                                 + Addr::Speech::kDispatcherOfs, &spk))
                return nullptr;
            return ValidSpeaker(spk) ? spk : nullptr;
        }

        // ------------------------------------------------------------ firing
        void NoteSuppressed(Event ev, const char* why) {
            R.suppressed++;
            const unsigned long now = GetTickCount();
            if (gCfg.RadioLogEvents && now - R.lastSuppressLogMs > 15000) {
                R.lastSuppressLogMs = now;
                Log::Verbose("[HeliRadioChat] Suppressed %s (%s; %u suppressed so far).",
                             kEventNames[ev], why, R.suppressed);
            }
        }

        // Rate gates that apply to every event.
        bool GateOk(Event ev) {
            if (R.globalCooldown > 0.0f) { NoteSuppressed(ev, "global cooldown"); return false; }
            if (R.eventCooldown[ev] > 0.0f) { NoteSuppressed(ev, "event cooldown"); return false; }
            if (R.linesThisMinute >= gCfg.RadioMaxLinesPerMinute) {
                NoteSuppressed(ev, "per-minute budget");
                return false;
            }
            return true;
        }

        void MarkPlayed(Event ev) {
            R.globalCooldown = gCfg.RadioGlobalCooldownSeconds;
            R.eventCooldown[ev] = gCfg.RadioEventCooldownSeconds;
            R.linesThisMinute++;
            if (gCfg.RadioLogEvents)
                Log::Info("[HeliRadioChat] Played %s.", kEventNames[ev]);
        }

        // Fire a helicopter-voiced event through its native trigger method.
        bool SayHeli(Event ev, uintptr_t fn, bool hasParam, int param) {
            if (!GateOk(ev)) return false;
            void* sp = SpeechSystem();
            if (!sp) {
                const unsigned long now = GetTickCount();
                if (now - R.lastNoSpeechLogMs > 30000) {
                    R.lastNoSpeechLogMs = now;
                    Log::Verbose("[HeliRadioChat] No pursuit speech system active; %s skipped.",
                                 kEventNames[ev]);
                }
                return false;
            }
            void* spk = HeliSpeaker(sp);
            if (!spk) { NoteSuppressed(ev, "no speaker available"); return false; }
            const bool ok = hasParam ? SafeIntArg(fn, spk, param)
                                     : SafeNoArg(fn, spk);
            if (!ok) {
                Log::Warn("[HeliRadioChat] Native call for %s faulted; radio chat disabled for safety.",
                          kEventNames[ev]);
                gVerified = false;
                return false;
            }
            MarkPlayed(ev);
            return true;
        }

        // Fire the dispatcher "helicopter inbound" line (no native trigger
        // method exists; scheduled exactly like the executable's other
        // dispatcher lines - argBytes=4, blob={speakerId}, source=speaker).
        bool SayDispatcherInbound() {
            const Event ev = EV_HELI_INBOUND;
            if (!GateOk(ev)) return false;
            void* sp = SpeechSystem();
            if (!sp) return false;
            void* spk = DispatcherSpeaker(sp);
            if (!spk) { NoteSuppressed(ev, "no dispatcher speaker"); return false; }
            int id = 0;
            if (!Memory::ReadBytes(reinterpret_cast<uintptr_t>(spk)
                                   + Addr::Speech::kSpeakerIdOfs,
                                   reinterpret_cast<uint8_t*>(&id), 4))
                return false;
            void* state = reinterpret_cast<void*>(Addr::Speech::kStateDispHeliBUETA);
            if (!SafeScheduleId(Addr::Speech::kSchedule, id,
                                Addr::Speech::kEntryDispHeliBUETA, state, spk)) {
                Log::Warn("[HeliRadioChat] Scheduler call faulted; radio chat disabled for safety.");
                gVerified = false;
                return false;
            }
            MarkPlayed(ev);
            return true;
        }

        // ------------------------------------------------------- validation
        struct GuardDef { uintptr_t va; const uint8_t* bytes; size_t len; const char* name; };

        bool VerifyGuards() {
            using namespace Addr;
            static const GuardDef defs[] = {
                { Speech::kSchedule,         Guard::SpeechSchedule,    sizeof(Guard::SpeechSchedule),    "ScheduleSpeechEvent" },
                { Speech::kGetSpeaker,       Guard::SpeechGetSpeaker,  sizeof(Guard::SpeechGetSpeaker),  "GetSpeaker" },
                { Speech::kSelfStrategyParam,Guard::SelfStrategyParam, sizeof(Guard::SelfStrategyParam), "HeliSelfStrategy(param)" },
                { Speech::kSelfStrategy2,    Guard::SelfStrategy2,     sizeof(Guard::SelfStrategy2),     "HeliSelfStrategy" },
                { Speech::kLostVisual,       Guard::LostVisual,        sizeof(Guard::LostVisual),        "HeliLostVisual" },
                { Speech::kIntentToBail,     Guard::IntentToBail,      sizeof(Guard::IntentToBail),      "HeliIntentToBail" },
                { Speech::kBailout,          Guard::Bailout,           sizeof(Guard::Bailout),           "HeliBailout" },
                { Speech::kSwarming,         Guard::Swarming,          sizeof(Guard::Swarming),          "HeliSwarming" },
                { Speech::kSpotter,          Guard::Spotter,           sizeof(Guard::Spotter),           "HeliSpotter" },
                { Speech::kHazardAlertParam, Guard::HazardAlertParam,  sizeof(Guard::HazardAlertParam),  "HeliHazardAlert(param)" },
                { Speech::kBullhornArrest,   Guard::BullhornArrest,    sizeof(Guard::BullhornArrest),    "HeliBullhornArrest" },
                { Speech::kQuadrentMoving,   Guard::QuadrentMoving,    sizeof(Guard::QuadrentMoving),    "HeliQuadrentMoving" },
                { Speech::kQuadrent,         Guard::Quadrent,          sizeof(Guard::Quadrent),          "HeliQuadrent" },
            };
            for (const GuardDef& d : defs) {
                if (!Memory::CheckBytes(d.va, d.bytes, d.len)) {
                    Log::Warn("[HeliRadioChat] Byte guard failed for %s at 0x%08X; "
                              "radio chat disabled (another mod may patch speech).",
                              d.name, static_cast<unsigned>(d.va));
                    return false;
                }
            }
            // The dispatcher line is scheduled by entry pointer: confirm the
            // name table still says this entry is Backup_DispHeliBUETA.
            void* namePtr = nullptr;
            char nameBuf[24] = {};
            if (!Memory::ReadPtr(Addr::Speech::kEntryDispHeliBUETA, &namePtr) || !namePtr
                || !Memory::ReadBytes(reinterpret_cast<uintptr_t>(namePtr),
                                      reinterpret_cast<uint8_t*>(nameBuf),
                                      sizeof(nameBuf) - 1)
                || std::strncmp(nameBuf, "Backup_DispHeliBUETA", 20) != 0) {
                Log::Warn("[HeliRadioChat] Speech event name table mismatch; radio chat disabled.");
                return false;
            }
            return true;
        }

        void ResetPursuit(void* ownerKey) {
            PursuitState fresh;
            fresh.ownerKey = ownerKey;
            fresh.lastMs = GetTickCount();
            R = fresh;
        }

    } // namespace

    // ---------------------------------------------------------------- public
    bool Initialize() {
        gShutdown = false;
        gVerified = false;
        if (!gCfg.EnableRadioChat) {
            Log::Info("[HeliRadioChat] Disabled in Radio.ini.");
            return false;
        }
        if (!VerifyGuards()) return false;
        gVerified = true;
        Log::Info("[HeliRadioChat] Native helicopter speech functions resolved.");
        Log::Info("[HeliRadioChat] Initialized (restored helicopter radio speech active).");
        return true;
    }

    void Reset() { ResetPursuit(nullptr); }

    void Shutdown() {
        gShutdown = true;
        gVerified = false;
        Reset();
    }

    bool Enabled() { return gVerified && !gShutdown && gCfg.EnableRadioChat; }

    void Update(const HeliState::Snapshot& s) {
        if (!Enabled()) return;

        const unsigned long now = GetTickCount();
        if (s.owner != R.ownerKey) {
            ResetPursuit(s.owner);
            return;
        }
        float dt = (now - R.lastMs) * 0.001f;
        R.lastMs = now;
        if (dt <= 0.0f || dt > 1.0f) return;
        dt = Clampf(dt, 0.001f, 0.25f);

        R.aliveTime += dt;
        if (R.globalCooldown > 0.0f) R.globalCooldown -= dt;
        for (int i = 0; i < EV_COUNT; ++i)
            if (R.eventCooldown[i] > 0.0f) R.eventCooldown[i] -= dt;
        R.minuteWindow += dt;
        if (R.minuteWindow >= 60.0f) { R.minuteWindow = 0.0f; R.linesThisMinute = 0; }

        // ----- independent player tracking (verified IPlayer chain) --------
        float ppos[3], pvel[3];
        const bool playerOk = HeliState::ReadPlayerKinematics(ppos, pvel);
        float dist = -1.0f, playerSpeed = -1.0f;
        if (playerOk) {
            const float dx = s.pos[0] - ppos[0], dz = s.pos[2] - ppos[2];
            dist = std::sqrt(dx * dx + dz * dz);
            playerSpeed = std::sqrt(pvel[0] * pvel[0] + pvel[1] * pvel[1]
                                    + pvel[2] * pvel[2]);
        }

        // Stopped-player hysteresis (independent of the AI layer).
        if (playerOk) {
            if (playerSpeed < 3.0f) {
                R.slowTime += dt;
                R.fastSamples = 0;
            } else if (playerSpeed > 4.5f && ++R.fastSamples >= 2) {
                R.slowTime = 0.0f;
                if (R.playerStopped) {
                    R.playerStopped = false;
                    R.stoppedTime = 0.0f;
                    R.arrestCloseTime = 0.0f;
                    const bool saidSomething = R.quadrentSaid || R.bullhornSaid;
                    R.quadrentSaid = false;
                    R.bullhornSaid = false;
                    // Pursuit resumes: position update from the helicopter.
                    if (gCfg.RadioPositionCalls && saidSomething)
                        SayHeli(EV_QUADRENT_MOVING, Addr::Speech::kQuadrentMoving,
                                false, 0);
                }
            }
            if (!R.playerStopped && R.slowTime >= 1.5f) R.playerStopped = true;
            if (R.playerStopped) R.stoppedTime += dt;
        }

        // The game's own visibility state, via the helicopter's pursuit
        // action: mode 1 means the helicopter is searching for the player.
        // -1 means the action is unknown; the distance heuristic then applies.
        const bool visValid = (s.heliMode >= 0);
        const bool hiddenNow = (s.heliMode == 1);
        if (visValid) {
            if (hiddenNow) { R.hiddenTime += dt; R.visibleTime = 0.0f; }
            else           { R.visibleTime += dt; R.hiddenTime = 0.0f; }
            // Quiet reacquire: if the spotter call is disabled or held by a
            // cooldown, do not leave the episode latched forever.
            if (R.lostEpisode && R.visibleTime >= 6.0f) R.lostEpisode = false;
        }

        // AI-layer signals (attack lifecycle, stuck recovery, aggression).
        AiDebug dbg;
        GetAiDebug(&dbg);

        // ----- triggers (edge-based; at most one line per tick) ------------

        // 1. Fuel exhausted -> pilot bailout call (only while the fuel timer
        //    actually removes the helicopter).
        if (gCfg.RadioFuelCalls && !gCfg.DisableFuelBasedExit && !R.bailoutSaid
            && s.fuel <= 0.5f && R.fuelPrev > 0.5f && R.aliveTime > 5.0f) {
            if (SayHeli(EV_BAILOUT, Addr::Speech::kBailout, false, 0))
                R.bailoutSaid = true;
        }
        // 2. Fuel low -> intent to bail.
        else if (gCfg.RadioFuelCalls && !gCfg.DisableFuelBasedExit && !R.fuelWarnSaid
                 && s.fuel > 0.5f && s.fuel < gCfg.RadioFuelWarnSeconds
                 && s.fuel < R.fuelPrev && R.aliveTime > 5.0f) {
            if (SayHeli(EV_INTENT_TO_BAIL, Addr::Speech::kIntentToBail, false, 0))
                R.fuelWarnSaid = true;
        }
        // 3. Dispatcher announces the helicopter shortly after it appears.
        else if (gCfg.RadioAnnounceArrival && !R.inboundSaid && R.aliveTime > 1.5f) {
            R.inboundSaid = true;   // one attempt per helicopter
            SayDispatcherInbound();
        }
        // 4. Pilot reports beginning to shadow the suspect.
        else if (gCfg.RadioAnnounceArrival && !R.arrivalSaid && R.aliveTime > 7.0f) {
            R.arrivalSaid = true;
            SayHeli(EV_ARRIVAL, Addr::Speech::kSelfStrategy2, false, 0);
        }
        // 5. Attack announced (optional; uses the strategy call).
        else if (gCfg.RadioAnnounceAttacks && dbg.attackActive && !R.attackPrev) {
            SayHeli(EV_ATTACK, Addr::Speech::kSelfStrategy2, false, 0);
        }
        // 6. Lost visual. Primary: the helicopter itself reports searching
        //    (its own line-of-sight state, the same one that sends it into
        //    search mode). Fallback when that state is unavailable: left far
        //    behind for a sustained time.
        else if (gCfg.RadioLostVisualCalls && !R.lostEpisode && visValid
                 && R.hiddenTime >= gCfg.RadioLostVisualSeconds) {
            R.lostEpisode = true;
            SayHeli(EV_LOST_VISUAL, Addr::Speech::kLostVisual, false, 0);
        }
        else if (gCfg.RadioLostVisualCalls && !visValid && playerOk && !R.lostEpisode
                 && dist > gCfg.RadioLostVisualDistance
                 && R.farTime >= gCfg.RadioLostVisualSeconds) {
            R.lostEpisode = true;
            SayHeli(EV_LOST_VISUAL, Addr::Speech::kLostVisual, false, 0);
        }
        // 7. Spotter call: first acquisition, or reacquired after losing you.
        //    Primary: the helicopter has you in sight again after searching.
        else if (gCfg.RadioSpotterCalls && visValid && !hiddenNow && playerOk
                 && R.visibleTime >= 0.75f
                 && (!R.spotterSaid || R.lostEpisode) && R.aliveTime > 3.0f) {
            const bool reacquired = R.lostEpisode;
            if (SayHeli(EV_SPOTTER, Addr::Speech::kSpotter, false, 0)) {
                R.spotterSaid = true;
                if (reacquired) R.lostEpisode = false;
            }
        }
        else if (gCfg.RadioSpotterCalls && !visValid && playerOk
                 && dist >= 0.0f && dist < 120.0f
                 && (!R.spotterSaid || R.lostEpisode) && R.aliveTime > 3.0f) {
            const bool reacquired = R.lostEpisode;
            if (SayHeli(EV_SPOTTER, Addr::Speech::kSpotter, false, 0)) {
                R.spotterSaid = true;
                if (reacquired) R.lostEpisode = false;
            }
        }
        // 8. Suspect stopped, helicopter overhead -> bullhorn arrest call.
        else if (gCfg.RadioArrestCalls && R.playerStopped && !R.bullhornSaid
                 && playerOk && dist >= 0.0f && dist < gCfg.RadioArrestDistance
                 && R.arrestCloseTime >= gCfg.RadioArrestSeconds) {
            if (SayHeli(EV_BULLHORN, Addr::Speech::kBullhornArrest, false, 0))
                R.bullhornSaid = true;
        }
        // 9. Suspect stopped -> position (quadrant) callout.
        else if (gCfg.RadioPositionCalls && R.playerStopped && !R.quadrentSaid
                 && R.stoppedTime >= gCfg.RadioStoppedCallSeconds) {
            if (SayHeli(EV_QUADRENT, Addr::Speech::kQuadrent, false, 0))
                R.quadrentSaid = true;
        }
        // 10. Helicopter obstructed (stuck recovery engaged) -> hazard alert.
        else if (gCfg.RadioHazardCalls
                 && dbg.state == AiState::StuckRecovery && !R.stuckPrev) {
            SayHeli(EV_HAZARD, Addr::Speech::kHazardAlertParam, true,
                    gCfg.RadioHazardContext);
        }
        // 11. Aggression fully ramped -> swarming call (optional).
        else if (gCfg.RadioSwarmCalls && !R.swarmSaid && gCfg.EnableDynamicAggression
                 && dbg.aggressionLevel >= 0.95f * gCfg.DynAggrMax
                 && gCfg.DynAggrMax > 0.05f) {
            if (SayHeli(EV_SWARM, Addr::Speech::kSwarming, false, 0))
                R.swarmSaid = true;
        }

        // ----- slow state updates ------------------------------------------
        if (playerOk && dist > gCfg.RadioLostVisualDistance) {
            R.farTime += dt;
        } else {
            R.farTime = 0.0f;
            if (!visValid && R.lostEpisode && playerOk && dist >= 0.0f
                && dist < 0.6f * gCfg.RadioLostVisualDistance)
                R.lostEpisode = false;   // reacquired quietly (no spotter line)
        }
        if (R.playerStopped && playerOk && dist >= 0.0f
            && dist < gCfg.RadioArrestDistance)
            R.arrestCloseTime += dt;
        else
            R.arrestCloseTime = 0.0f;

        R.fuelPrev = s.fuel;
        R.attackPrev = dbg.attackActive;
        R.stuckPrev = (dbg.state == AiState::StuckRecovery);
    }

} } // namespace Systems::HeliRadioChat
