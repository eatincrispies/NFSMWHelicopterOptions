// Systems.h - entry points for every feature system.
#pragma once

namespace Systems {

    // Patch appliers (called once from the init thread, in this order).
    void ApplySkidEntry();      // Skids.ini [SkidEntry]
    void ApplyLead();           // Leading.ini [ChopperLead]
    void ApplyAltitude();       // Altitude.ini [ChopperAltitude]
    void ApplySkidStrike();     // Skids.ini [SkidStrike]
    void ApplyAcceleration();   // Acceleration.ini [ChopperAcceleration]
    void ApplySteering();       // SteeringControl.ini [ChopperSteering]
    void ApplySmoothing();      // SteeringControl.ini [ChopperSmoothing]
    void ApplyVision();         // Visibility.ini [ChopperVision]
    void ApplyExitBehavior();   // AIHelicopterBehavior.ini [ExitBehavior]
    void ApplyHeliSheet();      // Navigation.ini [HeliSheet] (static patches)
    void ApplySpawner();        // Debug.ini [ChopperSpawner] (research gates)
    void ApplyHighFpsFix();     // Debug.ini [FrameRate] (200+ FPS motion loss)

    // Game-thread tick consumers. Order: AiTick shapes drive speed and live
    // floats first, then the speed regulator, then telemetry.
    void __cdecl SpeedRegulatorTick(void* heliThis);
    void __cdecl TelemetryTick(void* heliThis);

    bool AnyTickConsumerEnabled();
    void LogActiveSystems();

} // namespace Systems
