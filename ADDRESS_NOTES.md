# ADDRESS_NOTES — verified game addresses (NFSMW 1.3 speed.exe)

Executable identity: TimeDateStamp `0x438E4C8C`, ImageBase `0x00400000`,
SizeOfImage `0x00678E4E` (read from the OptionalHeader; an earlier note said
0x00A79000, which was a VA/RVA confusion in the audit script), SHA-256
`80774c2e5d619b4f120b48d4462896fd504c263399d203a238769cffde1d253c`, x86.
**Everything below was byte-verified against this executable** (83/83 guard
matches) and, where the function exists in the Ghidra dump, semantically
confirmed against the decompile and EA-derived source. The machine-readable
master copy is [`src/Core/Addresses.h`](HelicopterOptions/src/Core/Addresses.h);
the identity checker is [`tools/verify_exe.ps1`](tools/verify_exe.ps1).

## Cross-build findings (V2.1.1, via the nfsmw decompilation)

The WIP nfsmw decompilation (GC/X360/PS2 targets) was used as a SEMANTIC
reference only; every entry below was independently re-located in the PC 1.3
dump/exe. Labels: CH = cross-build hypothesis, PCL = PC candidate located,
SV = structurally verified, RV = runtime verified.

| Finding | PC location | Label |
|---|---|---|
| `IPlayer::First(PLAYER_LOCAL)` statics: count `0x0092D884`, head `0x0092D87C`; IPlayer vt+0x04 = `GetSimable` (first virtual per decomp header); simable vt+0x54 = `GetRigidBody` | used verbatim in StartPathToPlayerCar `0x00427770`; both globals byte-confirmed in the exe body | **SV** (RV pending first log) |
| `AICopManager::SpawnPursuitHelicopter` — gate on heli-involved ptr, CHOPPER cache lookup, spawn 250 m behind target (+20 m up), Activate→PlaceObject→SetSpawned→AddVehicle→count++ | `0x004269A0` (dump shows `IncNavPosition(0x437A0000=250.0f)`, `+20.0` via `DAT_00890560`, `-1.0` via `DAT_00890DA8`) | **SV** |
| `AICopManager::GetAvailableCopVehicleByClass(CHOPPER)` | `0x00426610` (returns "copheli" name path; called by 0x004269A0) | **SV** |
| `mNumActiveCopHelicopters` | AICopManager `+0x9C` (incremented at spawn) | **SV** |
| `mTotalCopsDestroyed` | AICopManager `+0xA8` ("cops_destroyed" stat) | **SV** |
| `mMaxActiveCopHelicopters` / `mMaxCopHelicopters` — BOTH initialized to **1** in the decomp constructor | PC constructor not yet located; fields expected near `+0x8C..0x98` | **CH/PCL pending** |
| `IRigidBody::PlaceObject(matrix, pos)` | rigid-body vtable `+0x98` (used by 0x004269A0) | **SV** |
| chopperspecs attribute layout — matches the PC SimpleChopper attrib block (`this+0xA4`) at five independent offsets: +0x1C PITCH_ALIGN_SCALE, +0x38 PITCH_ANG, +0x40 ROLL_ANG, **+0x44 MAX_SPEED_MPS**, +0x60 ROLL_ALIGN_SCALE | `FUN_006A2430` clamps desired-velocity magnitude against attrib+0x44 → **MAX_SPEED_MPS is the hidden top-speed clamp** | **SV** (override RV pending) |
| `WCollisionMgr` API (GetWorldHeightAtPoint[Rigorous], CheckHitWorld, GetBarrierNormal, GetObjectList/GetInstanceList, Collide) — the world-query foundation for Lv2 probes | PC functions not yet located | **CH** |
| Chopper cull distance 375.0 vs 600.0, `mCopMaxSpawnDist` 400.0 | search constants for locating the PC deactivation path | **CH** |

## Runtime finding: OnDriving `this` dispatch (V2.0.1)

The first runtime test showed that at the OnDriving prologue (0x00417A20)
**ECX does not hold the helicopter object** on the tested system (observed
float-like values ≈2.09, e.g. `0x400669A0`), contradicting the naive
__thiscall/Ghidra-__fastcall assumption. The hook therefore snapshots all
GPRs and structurally discovers the register holding a pointer that passes
the owner-chain validation (`+0x34` → `GetRigidBody` vt+0x54 → finite
state), then locks it and logs `[Hook] OnDriving 'this' located in <REG>`.
The **owner pointer (`aiThis+0x34`) is the stable per-helicopter identity**;
the constructor-time pointer, the OnDriving object, and `gHeliVehicle` must
never be compared to each other directly. The registry logs the
ctorPtr↔aiThis delta per instance to pin down the exact subobject
relationship — send that log line after the next test.

## Functions

| VA | Identity | Evidence |
|---|---|---|
| `0x00412770` | `AIActionHeliPursuit::StraightLinePursuit` | dump + EA source |
| `0x00412B40` | `AIActionHeliPursuit::SkidHitPursuit` | dump + EA source |
| `0x00417A20` | `AIVehicleHelicopter::OnDriving` (hook site, prologue `83 EC 68 53 55`) | dump + exe |
| `0x0041A5E0` | `AIVehicleHelicopter` constructor (hook site, prologue `6A FF 68 8E 7C 86 00`) | dump + exe |
| `0x00423510` | `AIVehicleHelicopter::UpdateFuel` | dump + EA source |
| `0x00423430` | Roadblock goal selector (heli vs static) | dump |
| `0x004269A0` | Special copheli spawn helper (gate at `0x004269CA`) | dump |
| `0x0042BA50` | Weighted pursuit-support selector (absent from dump; research snippet) | snippet + exe guards |
| `0x0043E8D0` | Pursuit spawn manager (cap branch `0x0043EB90`) | dump |
| `0x0040E720` | `ClearHeliVehicle()` helper: `mov [0x0090D8E4],0; ret` | exe bytes |
| `0x00424380` | Heli removal path (also clears `gHeliVehicle`) | dump |
| `0x006A2030` | SimpleChopper destination-velocity filter `(hist×4+in)×0.2` | dump + exe |
| `0x006A2430` | SimpleChopper motion update (accel budget, turn, smoothing) | dump + exe |
| `0x00739420` | `HeliRenderConn::Update` | dump |
| `0x007511E0` | `HeliRenderConn::OnRender` (reset write at `0x007511FC`) | dump + exe (byte-exact) |
| `0x006BA0B0` | HeliWash constructor (push `1.0f` at `0x006BA198`) | dump + exe |
| `0x00406E20` | Uses `Exit_Height` (`+= [0x008EB1F8]` on member `+0x68`) | dump |

`AIActionHeliPursuit::Update` (contains `0x00427872` vision branch and
`0x004278A5` sheet-reset write) and `AIActionHeliExit::Update` (contains the
`0x00427AA0–0x00427C29` float sites) sit in a **dump export gap**; their
guards are exe-verified and semantics EA-source-corroborated.

## Globals

| VA | Meaning | Vanilla |
|---|---|---|
| `0x0090D8E4` | `gHeliVehicle` (AIVehicleHelicopter*) — set unconditionally in ctor, cleared in dtor/removal | 0 |
| `0x0090D621` | `bIgnoreHeliSheet` — reset to 0 each pursuit tick at `0x004278A5`; true during skid | 0 |
| `0x0090D61C` | Heli-involved pursuit pointer (`pursuit−0x7A4` when heli joins) | 0 |
| `0x008EB1F8` | `Exit_Height` (.data float) | **25.0** |
| `0x008EB1F4` | `NeverIgnoreHeliSheet` — **UNVERIFIED** (no dump xref; TU-adjacency inference only) | 0 |
| `0x008F8DCC/D0/D4` | `Max_Chopper_Accel` / `Min_Chopper_Accel` / `Chopper_Ratio` — used by BOTH OnDriving decel and SimpleChopper budget | 80 / 30 / 2.0 |
| `0x0092C4F4` | UCrc32("CHOPPER") | — |
| `0x0090D8C8` | UCrc32("copheli") | — |

## Object offsets (PC layout, from the OnDriving decompile)

| Offset | Field |
|---|---|
| `this+0x34` | owner interface (vtable`+0x54` = `GetRigidBody`) |
| `this+0x84` | `mDriveSpeed` (AI desired speed) |
| `this+0x88` | `mDest` (drive target, Vector3) |
| `this+0x7AC` | `mDestinationVelocity` (Vector3) |
| `this+0x7B8` | `mLookAtPosition` (Vector3) |
| `this+0x7D8` | `mHeliFuelTimeRemaining` |
| `this+0x8B0` | `mISimpleChopper` (vt`+0x4` SetDesiredVelocity, `+0xC` MaxDeceleration, `+0x10` SetDesiredFacing) |
| rigidbody vt`+0x20` / `+0x24` | `GetPosition` / `GetLinearVelocity` (return Vector3*) |

Note: the EA-derived headers show different offsets (e.g. fuel at 0x808,
ISimpleChopper at 0x8BC) — the PC build is shifted. **Use only the PC
offsets above.** The three `HeliSheetCoordinate` members' PC offsets are
NOT yet verified (EA layout 0x814/0x84C/0x884 — do not use).

## Vanilla constants (.rdata, exe-read)

Skid/pursuit: cooldown −5.0 `[0x00890DB8]`, entry 5.0 `[0x00890DA4]` –
35.0 `[0x0089105C]`, dot 0.707 `[0x00891058]`, ydelta 13 `[0x00890648]`;
lead 0.4 `[0x00891054]` / 30 `[0x00890614]` / 45 `[0x00890658]` / 0.75
`[0x00891050]`; heights 2.0 `[0x00890D3C]` / 6.0 `[0x00891048]` / 12.0
`[0x00891044]`; side ±6.0 `[0x00891048]/[0x00891078]`; approach 0.23
`[0x0089064C]`, height 1.8 `[0x00891074]`, low-extra 3.0 `[0x00890604]`;
strike start 4.0 `[0x00890E98]`, lateral 1.9 `[0x00891070]`, vel 0.092
`[0x0089106C]`, back −0.5 `[0x00891068]`; aborts 1600 `[0x00891064]` /
144 `[0x00891060]`.

SimpleChopper: dest-vel filter 4.0 `[0x00890E98]` & 0.2 `[0x00895074]`;
accel speed scale 0.6 `[0x008AAE44]`; turn response −8 `[0x008AB8EC]`;
clamp ±1.3 `[0x008AAE5C]/[0x008AB8E8]`; smoothing 7.0 `[0x008A0718]` &
0.125 `[0x00890F14]`.

Exit: fly speed 100 (push imm at `0x00427BFE`), seek-up 15 `[0x00890DAC]`,
seek-ahead 85 `[0x00891610]`, reach 25 `[0x00890DC0]`, right −9
`[0x00891618]`, back −200 `[0x00891614]`, extra +5 `[0x00890DA4]`,
finish 22500 `[0x0089160C]`, `Exit_Height` 25 `[0x008EB1F8]`,
DoDriving mode 7 (push at `0x00427C28`).

## Shared-constant couplings (compiler constant merging)

The compiler pooled identical floats; one address can serve several call
sites. Per-instruction operand redirection keeps mod settings independent —
EXCEPT where the mod deliberately redirects two sites to one value:

- `0x00890DB8` (−5.0): skid cooldown gate AND chase-height mode threshold —
  both redirected to `SkidCooldownThreshold` (documented coupling; also used
  un-redirected by OnDriving's y-shaping).
- `0x00891048` (6.0): close chase height AND skid side offset (independent).
- `0x00890E98` (4.0): strike start distance AND dest-vel filter weight
  (independent).
- `0x00891054` (0.4): lead speed scale AND OnDriving brake factor AND strike
  y-offset (only the lead site is redirected).
- `0x00890DA4` (5.0): entry min distance AND exit flyout extra height
  (independent redirects).
- `0x00890DAC` (15.0): exit seek-up AND an OnDriving range check
  (only exit redirected).

## Sites deliberately not patched

Closing-speed constants inside StraightLinePursuit (20 `[0x00890560]`,
30 `[0x00890614]`, 50 `[0x00890DD0]`, 300-range `[0x00890544]`): shared
constants with un-audited co-users. Chase speed is shaped at runtime through
the drive-speed write instead (`[ChopperSpeed] MinChaseSpeed/MaxChaseSpeed`).

## Chopperspecs attribute block (unresolved)

`SimpleChopper this+0xA4` points to an attribute block (offsets +0x1C, +0x38,
+0x40, +0x44, +0x60 used in the motion math). Values come from the
`chopperspecs` attribute class; layout unresolved — needed for a true
engine-level speed unlock. Candidates for future runtime dumping.
