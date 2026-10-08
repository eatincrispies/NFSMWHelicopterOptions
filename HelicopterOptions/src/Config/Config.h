#pragma once

struct Config {
    float LeadBase                = 30.0f;
    float LeadMax                 = 45.0f;
    float LeadSmoothing           = 0.0f;

    float TurnClamp               = 1.3f;
    float TurnResponseScale       = -8.0f;
    float MaxChopperAccel         = 80.0f;
    float MinChopperAccel         = 30.0f;

    float SpeedCap                = 100.0f;
    float LineOfSight             = 0.0f;
    float FuelTime                = 0.0f;
    bool  HeliSheet               = true;
    float IgnoreHeliSheetDistance = 0.0f;

    float FlySpeed                = 100.0f;
    float SpawnDistance           = 250.0f;
};

extern Config gCfg;
