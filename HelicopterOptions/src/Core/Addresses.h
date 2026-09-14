#pragma once
#include <cstdint>

namespace Addr {

    struct FloatOperand {
        uintptr_t va;
        uint8_t   opcode[2];
        uintptr_t constant;
    };

    struct PushFloat {
        uintptr_t va;
        float     value;
    };

    constexpr uintptr_t kImageBase     = 0x00400000u;
    constexpr uint32_t  kTimeDateStamp = 0x438E4C8Cu;
    constexpr uint32_t  kSizeOfImage   = 0x00678E4Eu;

    constexpr uintptr_t gHeliVehicle         = 0x0090D61Cu;
    constexpr uintptr_t bIgnoreHeliSheet     = 0x0090D621u;
    constexpr uintptr_t NeverIgnoreHeliSheet = 0x008EB1F4u;

    namespace IPlayer {
        constexpr uintptr_t LocalListHead  = 0x0092D87Cu;
        constexpr uintptr_t LocalListCount = 0x0092D884u;
        constexpr unsigned  GetSimable     = 0x04u;
    }

    namespace ISimable {
        constexpr unsigned GetRigidBody = 0x54u;
    }

    namespace IRigidBody {
        constexpr unsigned GetPosition       = 0x20u;
        constexpr unsigned GetLinearVelocity = 0x24u;
    }

    namespace AIPerpVehicle {
        constexpr uintptr_t SetHeat            = 0x00409060u;
        constexpr uint8_t   SetHeatPrologue[7] = { 0x6A, 0xFF, 0x68, 0xBC, 0x79, 0x86, 0x00 };
        constexpr unsigned  Heat               = 0x1Cu;
        constexpr unsigned  IsRacing           = 0x2Eu;
    }

    namespace AIVehicleHelicopter {
        constexpr uintptr_t OnDriving            = 0x00417A20u;
        constexpr uint8_t   OnDrivingPrologue[5] = { 0x83, 0xEC, 0x68, 0x53, 0x55 };
        constexpr unsigned  Owner                = 0x34u;
        constexpr unsigned  DriveSpeed           = 0x84u;
        constexpr unsigned  FuelTimeRemaining    = 0x7D8u;
        constexpr unsigned  ISimpleChopper       = 0x8B0u;
    }

    namespace AIActionHeliPursuit {
        constexpr uintptr_t Constructor            = 0x00420EC0u;
        constexpr uint8_t   ConstructorPrologue[7] = { 0x6A, 0xFF, 0x68, 0xF8, 0x7D, 0x86, 0x00 };
        constexpr uintptr_t Vtable                 = 0x00891358u;
        constexpr unsigned  RigidBody              = 0x54u;
        constexpr unsigned  Mode                   = 0xA4u;

        constexpr FloatOperand SkidCooldownGate       = { 0x0041280Au, { 0xD8, 0x1D }, 0x00890DB8u };
        constexpr FloatOperand SkidCooldownHeightMode = { 0x004129C3u, { 0xD8, 0x1D }, 0x00890DB8u };
        constexpr FloatOperand SkidEntryMaxDistance   = { 0x00412849u, { 0xD8, 0x1D }, 0x0089105Cu };
        constexpr FloatOperand SkidEntryMinDistance   = { 0x0041285Eu, { 0xD8, 0x1D }, 0x00890DA4u };
        constexpr FloatOperand SkidEntryAlignment     = { 0x00412885u, { 0xD8, 0x1D }, 0x00891058u };
        constexpr FloatOperand SkidEntryMaxHeight     = { 0x004128A5u, { 0xD8, 0x1D }, 0x00890648u };
        constexpr FloatOperand LeadSpeedScale         = { 0x0041293Du, { 0xD8, 0x0D }, 0x00891054u };
        constexpr FloatOperand LeadBase               = { 0x00412946u, { 0xD8, 0x05 }, 0x00890614u };
        constexpr FloatOperand LeadMax                = { 0x0041294Cu, { 0xD8, 0x15 }, 0x00890658u };
        constexpr FloatOperand ChaseHeightSkid        = { 0x004129A7u, { 0xD8, 0x05 }, 0x00890D3Cu };
        constexpr FloatOperand ChaseHeightClose       = { 0x004129D4u, { 0xD8, 0x05 }, 0x00891048u };
        constexpr FloatOperand ChaseHeightHigh        = { 0x004129E0u, { 0xD8, 0x05 }, 0x00891044u };

        namespace Vanilla {
            constexpr float SkidCooldown         = -5.0f;
            constexpr float SkidEntryMaxDistance = 35.0f;
            constexpr float SkidEntryMinDistance = 5.0f;
            constexpr float SkidEntryAlignment   = 0.707f;
            constexpr float SkidEntryMaxHeight   = 13.0f;
            constexpr float LeadSpeedScale       = 0.4f;
            constexpr float LeadBase             = 30.0f;
            constexpr float LeadMax              = 45.0f;
            constexpr float ChaseHeightSkid      = 2.0f;
            constexpr float ChaseHeightClose     = 6.0f;
            constexpr float ChaseHeightHigh      = 12.0f;
        }
    }

    namespace SimpleChopper {
        constexpr uintptr_t InterfaceVtable    = 0x008AB86Cu;
        constexpr unsigned  ChopperSpecsLayout = 0x58u;

        constexpr FloatOperand TurnResponseScale  = { 0x006A28A8u, { 0xD8, 0x0D }, 0x008AB8ECu };
        constexpr FloatOperand TurnClampCompare   = { 0x006A28BAu, { 0xD8, 0x1D }, 0x008AAE5Cu };
        constexpr FloatOperand TurnClampLoad      = { 0x006A2994u, { 0xD9, 0x05 }, 0x008AAE5Cu };
        constexpr FloatOperand TurnClampNegativeA = { 0x006A28CFu, { 0xD9, 0x05 }, 0x008AB8E8u };
        constexpr FloatOperand TurnClampNegativeB = { 0x006A28E2u, { 0xD9, 0x05 }, 0x008AB8E8u };

        constexpr FloatOperand SmoothingOldWeight[3] = {
            { 0x006A28EFu, { 0xD8, 0x0D }, 0x008A0718u },
            { 0x006A28FCu, { 0xD8, 0x0D }, 0x008A0718u },
            { 0x006A2908u, { 0xD8, 0x0D }, 0x008A0718u },
        };
        constexpr FloatOperand SmoothingFinalScale[3] = {
            { 0x006A291Eu, { 0xD8, 0x0D }, 0x00890F14u },
            { 0x006A292Bu, { 0xD8, 0x0D }, 0x00890F14u },
            { 0x006A293Cu, { 0xD8, 0x0D }, 0x00890F14u },
        };
        constexpr FloatOperand DestinationOldWeight[3] = {
            { 0x006A204Fu, { 0xD8, 0x0D }, 0x00890E98u },
            { 0x006A205Bu, { 0xD8, 0x0D }, 0x00890E98u },
            { 0x006A2067u, { 0xD8, 0x0D }, 0x00890E98u },
        };
        constexpr FloatOperand DestinationFinalScale[3] = {
            { 0x006A2075u, { 0xD8, 0x0D }, 0x00895074u },
            { 0x006A2083u, { 0xD8, 0x0D }, 0x00895074u },
            { 0x006A2092u, { 0xD8, 0x0D }, 0x00895074u },
        };

        constexpr FloatOperand VelocityDeltaTimeGate = { 0x006A2762u, { 0xD8, 0x1D }, 0x00890EC4u };

        constexpr uintptr_t Max_Chopper_Accel = 0x008F8DCCu;
        constexpr uintptr_t Min_Chopper_Accel = 0x008F8DD0u;

        namespace Vanilla {
            constexpr float TurnResponseScale     = -8.0f;
            constexpr float TurnClamp             = 1.3f;
            constexpr float SmoothingOldWeight    = 7.0f;
            constexpr float SmoothingFinalScale   = 0.125f;
            constexpr float DestinationOldWeight  = 4.0f;
            constexpr float DestinationFinalScale = 0.2f;
            constexpr float VelocityDeltaTimeGate = 0.005f;
            constexpr float MaxChopperAccel       = 80.0f;
            constexpr float MinChopperAccel       = 30.0f;
        }
    }

    namespace chopperspecs {
        constexpr unsigned MAX_SPEED_MPS = 0x44u;

        namespace Vanilla {
            constexpr float MaxSpeedMps = 100.0f;
        }
    }

    namespace AIActionHeliExit {
        constexpr PushFloat FlySpeed = { 0x00427BFEu, 100.0f };
    }

    namespace AICopManager {
        constexpr PushFloat SpawnDistance = { 0x00426ABFu, 250.0f };
    }

}
