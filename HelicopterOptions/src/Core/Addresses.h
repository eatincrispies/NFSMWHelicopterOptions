// Addresses.h - single source of truth for every game address this mod touches.
//
// Target executable (the ONLY supported build):
//   speed.exe  NFSMW 1.3  x86  ImageBase 0x00400000
//   PE TimeDateStamp 0x438E4C8C   SizeOfImage 0x00678E4E
//   SHA-256 80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c
// (SizeOfImage read directly from the OptionalHeader; the earlier value
//  0x00A79000 was a VA/RVA confusion in the verification script.)
//
// Every entry below was verified against that executable byte-for-byte
// (83/83 guard matches, see AUDIT_REPORT.md Appendix B) and, where the
// function appears in the Ghidra dump, against the decompiled body.
//
// Evidence tags:
//   [EXE]    guard bytes / constant value read directly from speed.exe
//   [DUMP]   containing function identified in the Ghidra decompile
//   [SRC]    corroborated by EA-derived C++ source
//   [RUNTIME] still requires in-game verification (marked experimental)

#pragma once
#include <cstdint>

namespace Addr {

    // ---- executable identity -------------------------------------------- [EXE]
    constexpr uintptr_t kImageBase       = 0x00400000u;
    constexpr uint32_t  kTimeDateStamp   = 0x438E4C8Cu;
    constexpr uint32_t  kSizeOfImage     = 0x00678E4Eu;   // from OptionalHeader, runtime-confirmed

    // ---- confirmed functions -------------------------------------------- [DUMP][SRC]
    constexpr uintptr_t kStraightLinePursuit = 0x00412770u;  // AIActionHeliPursuit::StraightLinePursuit
    constexpr uintptr_t kSkidHitPursuit      = 0x00412B40u;  // AIActionHeliPursuit::SkidHitPursuit
    constexpr uintptr_t kOnDrivingHookVA     = 0x00417A20u;  // AIVehicleHelicopter::OnDriving prologue
    constexpr uintptr_t kHeliCtor            = 0x0041A5E0u;  // helicopter-spawn constructor (registry identity)
    // AIActionHeliPursuit constructor - the action object registers itself
    // under that literal name and stores the owner rigid body at +0x54.
    // Its live behavior mode at +0xA4 (0 chase, 1 search, 2/3 skid attack)
    // is the game's own lost-sight state, driven by IsPerpInSight inside the
    // per-tick dispatcher 0x004277E0 (vtable slot +0x1C).      [EXE][DUMP]
    constexpr uintptr_t kHeliActionCtor      = 0x00420EC0u;
    constexpr uintptr_t kUpdateFuel          = 0x00423510u;  // fuel at this+0x7D8 (PC layout)
    constexpr uintptr_t kDestVelFilterFn     = 0x006A2030u;  // (hist*4 + in) * 0.2 filter
    constexpr uintptr_t kSimpleChopperUpdate = 0x006A2430u;  // accel budget / turn / smoothing

    // ---- confirmed globals ---------------------------------------------- [DUMP][EXE]
    constexpr uintptr_t kGlobalHeliVehicle   = 0x0090D8E4u;  // gHeliVehicle; ctor sets, dtor clears
    constexpr uintptr_t kBIgnoreHeliSheet    = 0x0090D621u;  // reset each pursuit tick @0x004278A5
    constexpr uintptr_t kExitHeightVar       = 0x008EB1F8u;  // float Exit_Height = 25.0        [EXE]
    constexpr uintptr_t kNeverIgnoreHeliSheet = 0x008EB1F4u; // NO dump xref               [RUNTIME]
    constexpr uintptr_t kHeliInvolvedPtr     = 0x0090D61Cu;  // pursuit heli-involvement pointer
    constexpr uintptr_t kClearHeliHelper     = 0x0040E720u;  // mov [gHeliVehicle],0; ret       [EXE]

    // IPlayer::First(PLAYER_LOCAL) statics (ListableSet slot 1). Used verbatim
    // by StartPathToPlayerCar (0x00427770): count!=0 -> *head = IPlayer*.
    // Both addresses byte-confirmed inside 0x00427770's body in the exe. [DUMP][EXE]
    constexpr uintptr_t kPlayerLocalListHead  = 0x0092D87Cu;   // IPlayer**
    constexpr uintptr_t kPlayerLocalListCount = 0x0092D884u;   // int

    // AICopManager (PC candidates from the SpawnPursuitHelicopter decompile
    // match, FUN_004269A0):                                              [DUMP]
    constexpr uintptr_t kSpawnPursuitHelicopter    = 0x004269A0u;
    constexpr uintptr_t kGetAvailableCopVehByClass = 0x00426610u;
    constexpr unsigned  kCopMgrNumActiveHelis      = 0x9Cu;   // manager+0x9C++ on heli spawn
    constexpr unsigned  kCopMgrTotalCopsDestroyed  = 0xA8u;   // "cops_destroyed" stat write

    // Chopper accel globals (used by BOTH OnDriving decel check and the
    // SimpleChopper accel budget - documented coupling).                    [DUMP][EXE]
    constexpr uintptr_t kMaxChopperAccel     = 0x008F8DCCu;   // vanilla 80.0
    constexpr uintptr_t kMinChopperAccel     = 0x008F8DD0u;   // vanilla 30.0
    constexpr uintptr_t kChopperRatio        = 0x008F8DD4u;   // vanilla 2.0

    // ---- pursuit cop-speech system (HeliRadioChat) ----------------------- [EXE]
    // The game maps speech events by name through a static table of
    // { const char* name, u32 tag } pairs (139 entries, one per event in
    // copspeech.evt). A static initializer (0x006FD6B0) binds every entry to
    // a per-event state object via 0x0083196B. Events are scheduled through
    // 0x00713B20 (caller-cleaned: argBytes, argBlob, entry, state, source).
    // Each HeliSpecific_* event has a concrete __thiscall trigger method
    // (this = speaker object; blob[0] = [this+0xC]); those appear in the
    // 171-slot speaker interface vtable at 0x008B21EC and are dispatched
    // virtually by pursuit code (verified sites: 0x0071A291 SelfStrategy,
    // 0x0071F04C LostVisual, 0x0071FA0C/0x0071FE0C BullhornArrest).
    namespace Speech {
        constexpr uintptr_t kSystemPtr       = 0x00993CC8u;  // live Speech instance (null outside pursuit speech)
        constexpr uintptr_t kSchedule        = 0x00713B20u;  // ScheduleSpeechEvent(argBytes, blob, entry, state, source)
        constexpr uintptr_t kGetSpeaker      = 0x00715610u;  // __thiscall(Speech*, int role) -> speaker*; ret 4
        constexpr unsigned  kDispatcherOfs   = 0xD8u;        // [Speech+0xD8] = dispatcher speaker
        constexpr unsigned  kSpeakerVecBegin = 0x5Cu;        // {u32 key, speaker*} pairs, 8 bytes each
        constexpr unsigned  kSpeakerVecEnd   = 0x60u;
        constexpr unsigned  kSpeakerIdOfs    = 0x0Cu;        // [speaker+0xC] = id used as blob[0]

        // Event name-table entries ({name, tag} pairs in .data) and the
        // per-event state objects they are bound to at static-init time.
        constexpr uintptr_t kEntryDispHeliBUETA  = 0x00901CACu;  // "Backup_DispHeliBUETA"
        constexpr uintptr_t kStateDispHeliBUETA  = 0x009924C4u;

        // Concrete trigger methods (__thiscall on a speaker object). The
        // (param) variants take one stack int and clean it (ret 4).
        constexpr uintptr_t kSelfStrategyParam   = 0x00717B10u;  // HeliSpecific_HeliSelfStrategy(param)
        constexpr uintptr_t kSelfStrategy2       = 0x00717B40u;  // HeliSpecific_HeliSelfStrategy (const variant)
        constexpr uintptr_t kLostVisual          = 0x00717B70u;  // HeliSpecific_HeliLostVisual
        constexpr uintptr_t kIntentToBail        = 0x00717BC0u;  // HeliSpecific_HeliIntentToBail
        constexpr uintptr_t kBailout             = 0x00717C00u;  // HeliSpecific_HeliBailout
        constexpr uintptr_t kSwarming            = 0x00717C40u;  // HeliSpecific_HeliSwarming
        constexpr uintptr_t kSpotter             = 0x00717C70u;  // HeliSpecific_HeliSpotter
        constexpr uintptr_t kHazardAlertParam    = 0x00717CA0u;  // HeliSpecific_HeliHazardAlert(param)
        constexpr uintptr_t kBullhornArrest      = 0x00717CD0u;  // HeliSpecific_HeliBullhornArrest (self-gated)
        constexpr uintptr_t kQuadrentMoving      = 0x00717D10u;  // HeliSpecific_HeliQuadrentMoving
        constexpr uintptr_t kQuadrent            = 0x00717D40u;  // HeliSpecific_HeliQuadrent
    }

    // ---- verified object offsets (PC layout, from OnDriving decompile) --- [DUMP]
    namespace Heli {                       // AIVehicleHelicopter (behavior 'this')
        constexpr unsigned kOwner              = 0x34u;   // interface; vt+0x54 = GetRigidBody
        constexpr unsigned kDriveSpeed         = 0x84u;   // float, AI desired speed
        constexpr unsigned kDest               = 0x88u;   // Vector3 drive target
        constexpr unsigned kDestinationVel     = 0x7ACu;  // Vector3
        constexpr unsigned kLookAtPosition     = 0x7B8u;  // Vector3
        constexpr unsigned kFuelTimeRemaining  = 0x7D8u;  // float
        constexpr unsigned kISimpleChopper     = 0x8B0u;  // vt+0x4/0xC/0x10
    }
    namespace OwnerVt {
        constexpr unsigned kGetRigidBody       = 0x54u;
    }
    namespace PlayerVt {                   // IPlayer (decomp: GetSimable is the
        constexpr unsigned kGetSimable         = 0x04u;   // first virtual)  [DUMP][SRC]
    }
    namespace SimableVt {
        constexpr unsigned kGetRigidBody       = 0x54u;   // same slot as owner  [DUMP]
    }
    // SimpleChopper attribute block (this+0xA4 -> chopperspecs). Layout matches
    // the decomp header at five independently-used offsets (0x1C/0x38/0x40/
    // 0x44/0x60), so:                                       [DUMP + decomp layout]
    namespace ChopperSpecs {
        constexpr unsigned kPitchAlignScale    = 0x1Cu;
        constexpr unsigned kPitchAng           = 0x38u;
        constexpr unsigned kRollAng            = 0x40u;
        constexpr unsigned kMaxSpeedMps        = 0x44u;   // THE desired-velocity clamp
        constexpr unsigned kRollAlignScale     = 0x60u;
    }
    namespace HeliAction {                 // AIActionHeliPursuit (per-heli action)
        constexpr uintptr_t kVtable        = 0x00891358u; // written in the ctor
        constexpr unsigned  kRigidBody     = 0x54u;       // owner rigid body (identity match)
        constexpr unsigned  kMode          = 0xA4u;       // 0 chase, 1 search, 2/3 attack
    }
    namespace RigidBodyVt {
        constexpr unsigned kGetPosition        = 0x20u;   // returns const Vector3*
        constexpr unsigned kGetLinearVelocity  = 0x24u;   // returns const Vector3*
        constexpr unsigned kGetForwardVector   = 0x34u;   // (out Vector3*)     [DUMP: pursuit actions]
        constexpr unsigned kGetRightVector     = 0x38u;   // (out Vector3*)     [DUMP: pursuit actions]
    }

    // ---- vanilla constants read from speed.exe --------------------------- [EXE]
    namespace Vanilla {
        constexpr float SkidCooldown          = -5.0f;   // 0x00890DB8 (also height-mode threshold)
        constexpr float EntryMaxDistance      = 35.0f;   // 0x0089105C
        constexpr float EntryMinDistance      = 5.0f;    // 0x00890DA4 (also exit +5 extra height)
        constexpr float EntryAlignmentDot     = 0.707f;  // 0x00891058
        constexpr float EntryMaxHeightDelta   = 13.0f;   // 0x00890648
        constexpr float LeadSpeedScale        = 0.4f;    // 0x00891054 (also brake factor, strike y-offset)
        constexpr float LeadBase              = 30.0f;   // 0x00890614
        constexpr float LeadMax               = 45.0f;   // 0x00890658
        constexpr float LeadSkidMultiplier    = 0.75f;   // 0x00891050
        constexpr float ChaseHeightSkid       = 2.0f;    // 0x00890D3C (shared 2.0 constant)
        constexpr float ChaseHeightClose      = 6.0f;    // 0x00891048 (also skid side offset)
        constexpr float ChaseHeightHigh       = 12.0f;   // 0x00891044
        constexpr float SideOffsetPos         = 6.0f;    // 0x00891048
        constexpr float SideOffsetNeg         = -6.0f;   // 0x00891078
        constexpr float ApproachVelocityLead  = 0.23f;   // 0x0089064C
        constexpr float ApproachHeight        = 1.8f;    // 0x00891074
        constexpr float LowExtraHeight        = 3.0f;    // 0x00890604 (shared 3.0 constant)
        constexpr float StrikeStartDistance   = 4.0f;    // 0x00890E98 (also dest-vel filter weight)
        constexpr float StrikeLateralTrigger  = 1.9f;    // 0x00891070 (METERS, not a dot)
        constexpr float StrikeVelocityLead    = 0.092f;  // 0x0089106C
        constexpr float StrikeBackScale       = -0.5f;   // 0x00891068
        constexpr float AbortAheadSq          = 1600.0f; // 0x00891064
        constexpr float AbortBehindSq         = 144.0f;  // 0x00891060
        constexpr float DestVelFilterWeight   = 4.0f;    // 0x00890E98
        constexpr float DestVelFilterScale    = 0.2f;    // 0x00895074  gain (4+1)*0.2 = 1.0
        constexpr float AccelBudgetSpeedScale = 0.6f;    // 0x008AAE44
        constexpr float TurnResponseScale     = -8.0f;   // 0x008AB8EC
        constexpr float TurnClampPos          = 1.3f;    // 0x008AAE5C
        constexpr float TurnClampNeg          = -1.3f;   // 0x008AB8E8
        constexpr float SmoothingOldWeight    = 7.0f;    // 0x008A0718
        constexpr float SmoothingFinalScale   = 0.125f;  // 0x00890F14  gain (7+1)*0.125 = 1.0
        constexpr float MaxChopperAccel       = 80.0f;   // 0x008F8DCC (.data)
        constexpr float MinChopperAccel       = 30.0f;   // 0x008F8DD0 (.data)
        constexpr float ChopperRatio          = 2.0f;    // 0x008F8DD4 (.data)
        constexpr float ExitHeight            = 25.0f;   // 0x008EB1F8 (.data variable)
        constexpr float ExitFinishDistanceSq  = 22500.0f;// 0x0089160C
        constexpr float ExitSeekAhead         = 85.0f;   // 0x00891610
        constexpr float ExitReachDistanceSq   = 25.0f;   // 0x00890DC0
        constexpr float ExitRightScale        = -9.0f;   // 0x00891618
        constexpr float ExitFlyoutBackScale   = -200.0f; // 0x00891614
        constexpr float ExitSeekUpThreshold   = 15.0f;   // 0x00890DAC (shared 15.0 constant)
        constexpr float ExitFlyoutExtraHeight = 5.0f;    // 0x00890DA4 (shared 5.0 constant)
        constexpr float ExitFlySpeed          = 100.0f;  // push imm32 at 0x00427BFE
        constexpr float ChopperVelDtGate      = 0.005f;  // 0x00890EC4 (SHARED: 6 xrefs)
    }

    // ---- patch sites ------------------------------------------------------
    namespace Site {
        // StraightLinePursuit (0x00412770)                                   [DUMP][SRC]
        constexpr uintptr_t SkidCooldownGate     = 0x0041280Au;
        constexpr uintptr_t SkidCooldownHeight   = 0x004129C3u;
        constexpr uintptr_t EntryMaxDistance     = 0x00412849u;
        constexpr uintptr_t EntryMinDistance     = 0x0041285Eu;
        constexpr uintptr_t EntryAlignmentDot    = 0x00412885u;
        constexpr uintptr_t EntryMaxHeightDelta  = 0x004128A5u;
        constexpr uintptr_t LeadSpeedScale       = 0x0041293Du;
        constexpr uintptr_t LeadBase             = 0x00412946u;
        constexpr uintptr_t LeadMax              = 0x0041294Cu;
        constexpr uintptr_t LeadSkidMultiplier   = 0x00412965u;
        constexpr uintptr_t ChaseHeightSkid      = 0x004129A7u;
        constexpr uintptr_t ChaseHeightClose     = 0x004129D4u;
        constexpr uintptr_t ChaseHeightHigh      = 0x004129E0u;
        constexpr uintptr_t ForceSkidAttrBranch  = 0x00412805u;

        // SkidHitPursuit (0x00412B40)                                        [DUMP][SRC]
        constexpr uintptr_t SideOffsetPos        = 0x00412C3Eu;
        constexpr uintptr_t SideOffsetNeg        = 0x00412C57u;
        constexpr uintptr_t ApproachVelX         = 0x00412C87u;
        constexpr uintptr_t ApproachVelY         = 0x00412C97u;
        constexpr uintptr_t ApproachVelZ         = 0x00412CA7u;
        constexpr uintptr_t ApproachHeight       = 0x00412CB7u;
        constexpr uintptr_t LowExtraHeight       = 0x00412CCEu;
        constexpr uintptr_t StrikeStartDistance  = 0x00412D24u;
        constexpr uintptr_t StrikeLateralTrigger = 0x00412D4Fu;
        constexpr uintptr_t StrikeVelX           = 0x00412DADu;
        constexpr uintptr_t StrikeVelY           = 0x00412DB7u;
        constexpr uintptr_t StrikeVelZ           = 0x00412DC1u;
        constexpr uintptr_t StrikeBackX          = 0x00412DD1u;
        constexpr uintptr_t StrikeBackY          = 0x00412DE3u;
        constexpr uintptr_t StrikeBackZ          = 0x00412DF5u;
        constexpr uintptr_t AbortAheadSq         = 0x00412ED9u;
        constexpr uintptr_t AbortBehindSq        = 0x00412EFCu;

        // SimpleChopper dest-velocity filter (0x006A2030)                    [DUMP]
        constexpr uintptr_t DestVelWeightX       = 0x006A204Fu;
        constexpr uintptr_t DestVelWeightY       = 0x006A205Bu;
        constexpr uintptr_t DestVelWeightZ       = 0x006A2067u;
        constexpr uintptr_t DestVelScaleX        = 0x006A2075u;
        constexpr uintptr_t DestVelScaleY        = 0x006A2083u;
        constexpr uintptr_t DestVelScaleZ        = 0x006A2092u;

        // SimpleChopper motion update (0x006A2430)                           [DUMP]
        constexpr uintptr_t AccelBudgetSpeedScale = 0x006A2579u;
        constexpr uintptr_t ForceMaxAccelBranch   = 0x006A25A7u;
        constexpr uintptr_t TurnResponseScale     = 0x006A28A8u;
        constexpr uintptr_t TurnClampPosCmp       = 0x006A28BAu;
        constexpr uintptr_t TurnClampNegLoadA     = 0x006A28CFu;
        constexpr uintptr_t TurnClampNegLoadB     = 0x006A28E2u;
        constexpr uintptr_t SmoothOldWeightX      = 0x006A28EFu;
        constexpr uintptr_t SmoothOldWeightY      = 0x006A28FCu;
        constexpr uintptr_t SmoothOldWeightZ      = 0x006A2908u;
        constexpr uintptr_t SmoothFinalScaleX     = 0x006A291Eu;
        constexpr uintptr_t SmoothFinalScaleY     = 0x006A292Bu;
        constexpr uintptr_t SmoothFinalScaleZ     = 0x006A293Cu;
        constexpr uintptr_t TurnClampPosLoad      = 0x006A2994u;
        // The chopper update derives its own velocity as (position delta / dt)
        // and only does so when dt exceeds this threshold. Vanilla 0.005 s =
        // 200 FPS: above that the derivation is skipped, the destination-
        // velocity filter is never fed, and the X/Z motion channels collapse
        // to zero. Redirecting THIS OPERAND (the 0.005 constant itself is
        // shared with five unrelated sites) lowers the threshold.   [EXE][DUMP]
        constexpr uintptr_t ChopperVelDtGate      = 0x006A2762u;

        // UpdateFuel (0x00423510)                                            [DUMP][SRC]
        constexpr uintptr_t FuelExitBranch        = 0x0042352Eu;  // JP +0x4C -> EB

        // AIActionHeliPursuit::Update region (dump gap; exe-verified guards) [EXE][SRC]
        constexpr uintptr_t VisionSightBranch     = 0x00427872u;
        constexpr uintptr_t VisionSightJnz        = 0x00427874u;
        constexpr uintptr_t SheetResetWrite       = 0x004278A5u;
        constexpr uintptr_t SheetResetImm         = 0x004278ABu;

        // AIActionHeliExit region (dump gap; exe-verified guards)            [EXE][SRC]
        constexpr uintptr_t ExitFinishHeight      = 0x0042794Eu;
        constexpr uintptr_t ExitFinishDistanceSq  = 0x004279C8u;
        constexpr uintptr_t ExitReachDistanceSq   = 0x00427AA0u;
        constexpr uintptr_t ExitRightScale        = 0x00427AF8u;
        constexpr uintptr_t ExitBackX             = 0x00427B34u;
        constexpr uintptr_t ExitBackY             = 0x00427B45u;
        constexpr uintptr_t ExitBackZ             = 0x00427B56u;
        constexpr uintptr_t ExitFlyoutExtraHeight = 0x00427B74u;
        constexpr uintptr_t ExitSeekUpThreshold   = 0x00427B84u;
        constexpr uintptr_t ExitSeekAheadX        = 0x00427B95u;
        constexpr uintptr_t ExitSeekAheadY        = 0x00427BA6u;
        constexpr uintptr_t ExitSeekAheadZ        = 0x00427BB7u;
        constexpr uintptr_t ExitSeekCarHeight     = 0x00427BC4u;
        constexpr uintptr_t ExitFlySpeedPush      = 0x00427BFEu;
        constexpr uintptr_t ExitFlySpeedImm       = 0x00427BFFu;
        constexpr uintptr_t ExitDoDrivingPush     = 0x00427C28u;
        constexpr uintptr_t ExitDoDrivingImm      = 0x00427C29u;

        // Dispatch / spawn gates ([ChopperSpawner] research)                 [EXE]
        constexpr uintptr_t CopheliWeightGate     = 0x0042BC54u;
        constexpr uintptr_t SelectorHeliGate      = 0x0042BB04u;
        constexpr uintptr_t SpecialSpawnHeliGate  = 0x004269CAu;  // tests kHeliInvolvedPtr==0 [DUMP]
        constexpr uintptr_t ForceSelectorCopheli  = 0x0042BB5Au;
        constexpr uintptr_t SpawnCapBranch        = 0x0043EB90u;  // affects ALL vehicle types [DUMP]
    }

    // ---- byte guards (exact bytes present in the supported exe) ---------- [EXE]
    namespace Guard {
        constexpr uint8_t SkidCooldown[6]      = { 0xD8,0x1D,0xB8,0x0D,0x89,0x00 };
        constexpr uint8_t EntryMaxDist[6]      = { 0xD8,0x1D,0x5C,0x10,0x89,0x00 };
        constexpr uint8_t EntryMinDist[6]      = { 0xD8,0x1D,0xA4,0x0D,0x89,0x00 };
        constexpr uint8_t EntryDot[6]          = { 0xD8,0x1D,0x58,0x10,0x89,0x00 };
        constexpr uint8_t EntryYDelta[6]       = { 0xD8,0x1D,0x48,0x06,0x89,0x00 };
        constexpr uint8_t LeadSpeedScale[6]    = { 0xD8,0x0D,0x54,0x10,0x89,0x00 };
        constexpr uint8_t LeadBase[6]          = { 0xD8,0x05,0x14,0x06,0x89,0x00 };
        constexpr uint8_t LeadMax[6]           = { 0xD8,0x15,0x58,0x06,0x89,0x00 };
        constexpr uint8_t LeadSkidMult[6]      = { 0xD8,0x0D,0x50,0x10,0x89,0x00 };
        constexpr uint8_t HeightSkid[6]        = { 0xD8,0x05,0x3C,0x0D,0x89,0x00 };
        constexpr uint8_t HeightClose[6]       = { 0xD8,0x05,0x48,0x10,0x89,0x00 };
        constexpr uint8_t HeightHigh[6]        = { 0xD8,0x05,0x44,0x10,0x89,0x00 };
        constexpr uint8_t SidePos[6]           = { 0xD9,0x05,0x48,0x10,0x89,0x00 };
        constexpr uint8_t SideNeg[6]           = { 0xD9,0x05,0x78,0x10,0x89,0x00 };
        constexpr uint8_t ApproachVel[6]       = { 0xD8,0x0D,0x4C,0x06,0x89,0x00 };
        constexpr uint8_t ApproachHeight[6]    = { 0xD8,0x05,0x74,0x10,0x89,0x00 };
        constexpr uint8_t LowExtra[6]          = { 0xD8,0x05,0x04,0x06,0x89,0x00 };
        constexpr uint8_t StrikeDist[6]        = { 0xD8,0x1D,0x98,0x0E,0x89,0x00 };
        constexpr uint8_t StrikeLateral[6]     = { 0xD8,0x1D,0x70,0x10,0x89,0x00 };
        constexpr uint8_t StrikeVel[6]         = { 0xD8,0x0D,0x6C,0x10,0x89,0x00 };
        constexpr uint8_t StrikeBack[6]        = { 0xD8,0x0D,0x68,0x10,0x89,0x00 };
        constexpr uint8_t AbortAhead[6]        = { 0xD8,0x1D,0x64,0x10,0x89,0x00 };
        constexpr uint8_t AbortBehind[6]       = { 0xD8,0x1D,0x60,0x10,0x89,0x00 };
        constexpr uint8_t DestVelWeight[6]     = { 0xD8,0x0D,0x98,0x0E,0x89,0x00 };
        constexpr uint8_t DestVelScale[6]      = { 0xD8,0x0D,0x74,0x50,0x89,0x00 };
        constexpr uint8_t AccelSpeedScale[6]   = { 0xD8,0x0D,0x44,0xAE,0x8A,0x00 };
        constexpr uint8_t TurnResponse[6]      = { 0xD8,0x0D,0xEC,0xB8,0x8A,0x00 };
        constexpr uint8_t TurnClampPosCmp[6]   = { 0xD8,0x1D,0x5C,0xAE,0x8A,0x00 };
        constexpr uint8_t TurnClampPosLoad[6]  = { 0xD9,0x05,0x5C,0xAE,0x8A,0x00 };
        constexpr uint8_t TurnClampNegLoad[6]  = { 0xD9,0x05,0xE8,0xB8,0x8A,0x00 };
        constexpr uint8_t SmoothOldWeight[6]   = { 0xD8,0x0D,0x18,0x07,0x8A,0x00 };
        constexpr uint8_t SmoothFinalScale[6]  = { 0xD8,0x0D,0x14,0x0F,0x89,0x00 };
        constexpr uint8_t ChopperVelDtGate[6]  = { 0xD8,0x1D,0xC4,0x0E,0x89,0x00 };
        constexpr uint8_t ExitFinishHeight[6]  = { 0xD8,0x1D,0xF8,0xB1,0x8E,0x00 };
        constexpr uint8_t ExitFinishDistSq[6]  = { 0xD8,0x1D,0x0C,0x16,0x89,0x00 };
        constexpr uint8_t ExitSeekUp[6]        = { 0xD8,0x1D,0xAC,0x0D,0x89,0x00 };
        constexpr uint8_t ExitSeekAhead[6]     = { 0xD8,0x0D,0x10,0x16,0x89,0x00 };
        constexpr uint8_t ExitSeekCarHeight[6] = { 0xD9,0x05,0xF8,0xB1,0x8E,0x00 };
        constexpr uint8_t ExitReachDistSq[6]   = { 0xD8,0x1D,0xC0,0x0D,0x89,0x00 };
        constexpr uint8_t ExitRightScale[6]    = { 0xD9,0x05,0x18,0x16,0x89,0x00 };
        constexpr uint8_t ExitBackScale[6]     = { 0xD8,0x0D,0x14,0x16,0x89,0x00 };
        constexpr uint8_t ExitExtraHeight[6]   = { 0xD8,0x05,0xA4,0x0D,0x89,0x00 };

        constexpr uint8_t ForceSkidAttr[2]     = { 0x74,0x14 };
        constexpr uint8_t ForceMaxAccel[2]     = { 0x74,0x0A };
        constexpr uint8_t FuelExitBranch[2]    = { 0x7A,0x4C };
        constexpr uint8_t VisionBranch[4]      = { 0x84,0xC0,0x75,0x1C };
        constexpr uint8_t SheetResetWrite[7]   = { 0xC6,0x05,0x21,0xD6,0x90,0x00,0x00 };
        constexpr uint8_t ExitFlySpeedPush[5]  = { 0x68,0x00,0x00,0xC8,0x42 };
        constexpr uint8_t ExitDoDrivingPush[2] = { 0x6A,0x07 };
        constexpr uint8_t CopheliWeightGate[2] = { 0x75,0x0E };
        constexpr uint8_t SelectorHeliGate[2]  = { 0x75,0x4C };
        constexpr uint8_t SpecialSpawnGate[6]  = { 0x0F,0x85,0x1B,0x02,0x00,0x00 };
        constexpr uint8_t ForceSelector[2]     = { 0x74,0x17 };
        constexpr uint8_t SpawnCapBranch[2]    = { 0x7D,0x51 };
        constexpr uint8_t OnDrivingPrologue[5] = { 0x83,0xEC,0x68,0x53,0x55 };
        // AIVehicleHelicopter ctor: push -1; push 0x00867C8E (7 relocatable bytes)
        constexpr uint8_t HeliCtorPrologue[7]  = { 0x6A,0xFF,0x68,0x8E,0x7C,0x86,0x00 };
        // AIActionHeliPursuit ctor: push -1; push 0x00867DF8 (same shape)
        constexpr uint8_t HeliActionCtorPrologue[7] = { 0x6A,0xFF,0x68,0xF8,0x7D,0x86,0x00 };

        // Cop-speech functions (first bytes; guard before any call is made).
        constexpr uint8_t SpeechSchedule[8]    = { 0x64,0xA1,0x00,0x00,0x00,0x00,0x6A,0xFF };
        constexpr uint8_t SpeechGetSpeaker[7]  = { 0x6A,0xFF,0x68,0x90,0x01,0x88,0x00 };
        constexpr uint8_t SelfStrategyParam[8] = { 0x83,0xEC,0x08,0x8B,0x41,0x0C,0x8B,0x54 };
        constexpr uint8_t SelfStrategy2[8]     = { 0x83,0xEC,0x08,0x8B,0x41,0x0C,0x51,0x68 };
        constexpr uint8_t LostVisual[8]        = { 0x83,0xEC,0x08,0xA1,0xC8,0x3C,0x99,0x00 };
        constexpr uint8_t IntentToBail[8]      = { 0x83,0xEC,0x08,0x56,0x8B,0xF1,0x8B,0x46 };
        constexpr uint8_t Bailout[8]           = { 0x83,0xEC,0x08,0x56,0x8B,0xF1,0x8B,0x46 };
        constexpr uint8_t Swarming[8]          = { 0x51,0x8B,0x41,0x0C,0x51,0x68,0x74,0x24 };
        constexpr uint8_t Spotter[8]           = { 0x51,0x8B,0x41,0x0C,0x51,0x68,0x64,0x26 };
        constexpr uint8_t HazardAlertParam[8]  = { 0x83,0xEC,0x08,0x8B,0x41,0x0C,0x8B,0x54 };
        constexpr uint8_t BullhornArrest[8]    = { 0x51,0x56,0x8B,0xF1,0xE8,0xF7,0xD3,0xFE };
        constexpr uint8_t QuadrentMoving[8]    = { 0x51,0x8B,0x41,0x0C,0x51,0x68,0x04,0x24 };
        constexpr uint8_t Quadrent[8]          = { 0x51,0x8B,0x41,0x0C,0x51,0x68,0x54,0x26 };
    }

} // namespace Addr
