#pragma once
#include "../Core/Addresses.h"

struct Config {
    float LeadSpeedScale       = Addr::AIActionHeliPursuit::Vanilla::LeadSpeedScale;
    float LeadBase             = Addr::AIActionHeliPursuit::Vanilla::LeadBase;
    float LeadMax              = Addr::AIActionHeliPursuit::Vanilla::LeadMax;
    float ChaseHeightSkid      = Addr::AIActionHeliPursuit::Vanilla::ChaseHeightSkid;
    float ChaseHeightClose     = Addr::AIActionHeliPursuit::Vanilla::ChaseHeightClose;
    float ChaseHeightHigh      = Addr::AIActionHeliPursuit::Vanilla::ChaseHeightHigh;
    float SkidCooldown         = Addr::AIActionHeliPursuit::Vanilla::SkidCooldown;
    float SkidEntryMinDistance = Addr::AIActionHeliPursuit::Vanilla::SkidEntryMinDistance;
    float SkidEntryMaxDistance = Addr::AIActionHeliPursuit::Vanilla::SkidEntryMaxDistance;
    float SkidEntryAlignment   = Addr::AIActionHeliPursuit::Vanilla::SkidEntryAlignment;
    float SkidEntryMaxHeight   = Addr::AIActionHeliPursuit::Vanilla::SkidEntryMaxHeight;
    float ReattackDelay        = 0.0f;

    float TurnResponseScale    = Addr::SimpleChopper::Vanilla::TurnResponseScale;
    float TurnClamp            = Addr::SimpleChopper::Vanilla::TurnClamp;
    float MaxChopperAccel      = Addr::SimpleChopper::Vanilla::MaxChopperAccel;
    float MinChopperAccel      = Addr::SimpleChopper::Vanilla::MinChopperAccel;

    float SpeedCap             = Addr::chopperspecs::Vanilla::MaxSpeedMps;
    float MaxVerticalSpeed     = 0.0f;
    float FuelTime             = 0.0f;
    bool  HeliSheet            = true;
    float IgnoreHeliSheetDistance = 0.0f;

    float FlySpeed             = Addr::AIActionHeliExit::FlySpeed.value;
    float SpawnDistance        = Addr::AICopManager::SpawnDistance.value;
};

extern Config gCfg;
