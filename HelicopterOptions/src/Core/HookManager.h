// HookManager.h - the two code hooks this mod installs.
//
// 1) AIVehicleHelicopter::OnDriving (0x00417A20) prologue -> per-frame AI
//    tick. The helicopter object register is DISCOVERED at runtime by
//    structural validation of all GPRs (runtime test #1 proved ECX does not
//    hold it on at least one system), then locked and logged.
// 2) AIVehicleHelicopter constructor (0x0041A5E0) prologue -> lifecycle
//    observation for the HelicopterRegistry (identity only).
// 3) AIActionHeliPursuit constructor (0x00420EC0) prologue -> records the
//    per-helicopter action object so its live behavior mode (the game's own
//    chase/search/attack state) can be read. Fires once per spawn.
//
// Both hooks preserve EFLAGS + all GPRs and call plain-C dispatchers. At a
// function prologue the x87 stack is empty per the x86 ABI and XMM registers
// are volatile, so C++ code here cannot corrupt live game FPU state.
#pragma once

namespace Hook {

    typedef void (__cdecl* TickFn)(void* heliThis);

    bool RegisterTick(TickFn fn);          // max 8, called in order
    bool InstallOnDrivingHook();
    bool InstallHeliCtorHook(TickFn fn);
    bool InstallHeliActionCtorHook(TickFn fn);   // AIActionHeliPursuit spawn

    // Restore original prologues. Trampolines are intentionally leaked
    // (a game thread could be executing them).
    void Remove();

} // namespace Hook
