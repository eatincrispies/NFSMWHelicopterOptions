#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include "AIActionHeliPursuit.hpp"
#include "AIVehicleHelicopter.hpp"

namespace {

    constexpr uintptr_t kVTable = 0x00891358u;

    constexpr float    kTwoPi                = 6.2831853f;
    constexpr float    kSearchNextPointRange = 30.0f;
    constexpr float    kSearchRadiusGrowth   = 40.0f;
    constexpr float    kSearchRadiusJitter   = 0.25f;
    constexpr float    kSearchSmallestStep   = 0.08f;
    constexpr float    kSearchLargestStep    = 0.17f;
    constexpr float    kSearchConeFirst      = 0.17f;
    constexpr float    kSearchConeLargest    = 0.5f;
    constexpr float    kSearchHeightNear     = 5.0f;
    constexpr float    kSearchHeightFar      = 25.0f;
    constexpr int      kPointsPerHover       = 3;
    constexpr float    kHoverShortest        = 3.0f;
    constexpr float    kHoverLongest         = 5.0f;
    constexpr float    kHoverLookDrop        = 20.0f;
    constexpr float    kLongestSearchStep    = 1.0f;
    constexpr float    kGroundUnitRange      = 400.0f;
    constexpr float    kGroundUnitSide       = 0.5f;
    constexpr int      kMostGroundUnits      = 32;
    constexpr int      kMostVisitedSpots     = 8;
    constexpr int      kMostZonesScanned     = 4096;
    constexpr float    kEscapeHeadingSpeed   = 5.0f;
    constexpr float    kReverseSpeed         = 2.0f;
    constexpr float    kFarSeek              = 300.0f;
    constexpr float    kMidSeek              = 15.0f;
    constexpr float    kNearSeek             = 3.0f;
    constexpr float    kAheadReachSq         = 1600.0f;
    constexpr float    kBehindReachSq        = 144.0f;
    constexpr float    kLookSweepAngle       = 0.79f;
    constexpr float    kLookSweepPeriod      = 10.0f;
    constexpr float    kLookDistance         = 60.0f;
    constexpr unsigned kSearchDrivingFlags   = 7u;
    constexpr float    kStrikeLead           = 0.092f;
    constexpr float    kStrikeCrossing       = 0.5f;
    constexpr float    kSkidHitSide          = 6.0f;
    constexpr float    kLineUpDistance       = 5.0f;
    constexpr float    kStrikeHeight         = 0.45f;
    constexpr float    kCoverLookAhead       = 0.5f;
    constexpr float    kSkidHitNotYet        = -4.9f;
    constexpr float    kSweepHeight          = 1.5f;
    constexpr float    kStrikeReach          = 25.0f;
    constexpr float    kContactOverlap       = 0.15f;
    constexpr float    kBounceSide           = 6.0f;
    constexpr float    kBounceHeight         = 3.0f;
    constexpr float    kUnderCar             = -2.0f;
    constexpr float    kStrikeHold           = 0.25f;
    constexpr float    kLongestStrike        = 4.0f;
    constexpr float    kAfterHit             = 0.3f;
    constexpr float    kApproachLead         = 0.23f;
    constexpr float    kGameStrikeStart      = 4.0f;
    constexpr float    kStrikeStartPerSpeed  = 0.12f;
    constexpr float    kSideClearance        = 1.9f;
    constexpr float    kApproachPatience     = 2.0f;
    constexpr float    kGameStrikeTime       = 2.0f;
    constexpr uintptr_t kSoundAI_mInstance   = 0x00993CC8u;
    constexpr unsigned kSoundAIHeli          = 0xE0u;
    constexpr unsigned kIntentToRam          = 0xF8u;
    constexpr uintptr_t kSim_GetTime         = 0x006E8DE0u;
    constexpr float    kSquadTimeout         = 1.0f;
    constexpr float    kTurnGap              = 2.0f;
    constexpr float    kLeadPatience         = 15.0f;
    constexpr float    kShortestLead         = 1.0f;

    constexpr int kTrackedActions = 16;
    constexpr int kMostInSquad    = 8;

    enum eTrackPathZoneType {
        TRACK_PATH_ZONE_TUNNEL         = 3,
        TRACK_PATH_ZONE_OVERPASS       = 4,
        TRACK_PATH_ZONE_OVERPASS_SMALL = 5,
        TRACK_PATH_ZONE_HIDDEN         = 8,
    };

    constexpr eTrackPathZoneType kHidingZoneTypes[] = { TRACK_PATH_ZONE_HIDDEN, TRACK_PATH_ZONE_TUNNEL, TRACK_PATH_ZONE_OVERPASS,
                                                        TRACK_PATH_ZONE_OVERPASS_SMALL };

    constexpr uintptr_t kTheTrackPathManager    = 0x009AA540u;
    constexpr unsigned  kZoneInfoTable          = 0x0Cu;
    constexpr unsigned  kZoneInfoSize           = 0x4Cu;
    constexpr unsigned  kZoneInfoNumZones       = 0x00u;
    constexpr unsigned  kZoneInfoFirstZone      = 0x04u;
    constexpr unsigned  kZoneInfoLastZone       = 0x08u;
    constexpr unsigned  kZoneElevation          = 0x14u;
    constexpr unsigned  kZoneBBoxMin            = 0x20u;
    constexpr unsigned  kZoneMemoryImageSize    = 0x42u;
    constexpr unsigned  kPursuitVehicles        = 0x18u;
    constexpr unsigned  kPursuitNumVehicles     = 0x20u;
    constexpr unsigned  kIVehicleGetPosition    = 0x0Cu;

    static_assert(offsetof(AIActionHeliPursuit, mIVehicleAI) == 0x4C, "AIActionHeliPursuit::mIVehicleAI");
    static_assert(offsetof(AIActionHeliPursuit, mIRigidBody) == 0x54, "AIActionHeliPursuit::mIRigidBody");
    static_assert(offsetof(AIActionHeliPursuit, mIAIHelicopter) == 0x58, "AIActionHeliPursuit::mIAIHelicopter");
    static_assert(offsetof(AIActionHeliPursuit, mPursuitTime) == 0x60, "AIActionHeliPursuit::mPursuitTime");
    static_assert(offsetof(AIActionHeliPursuit, mSkidKnockTimer) == 0x64, "AIActionHeliPursuit::mSkidKnockTimer");
    static_assert(offsetof(AIActionHeliPursuit, mPathTime) == 0x68, "AIActionHeliPursuit::mPathTime");
    static_assert(offsetof(AIActionHeliPursuit, mBuildingPath) == 0x6C, "AIActionHeliPursuit::mBuildingPath");
    static_assert(offsetof(AIActionHeliPursuit, mSearchPatternAngle) == 0x70, "AIActionHeliPursuit::mSearchPatternAngle");
    static_assert(offsetof(AIActionHeliPursuit, mSearchDestPoint) == 0x74, "AIActionHeliPursuit::mSearchDestPoint");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerRigidBody) == 0x80, "AIActionHeliPursuit::mPlayerRigidBody");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerPosition) == 0x84, "AIActionHeliPursuit::mPlayerPosition");
    static_assert(offsetof(AIActionHeliPursuit, mSkidHitOffset) == 0x90, "AIActionHeliPursuit::mSkidHitOffset");
    static_assert(offsetof(AIActionHeliPursuit, mCollisionAbort) == 0x9C, "AIActionHeliPursuit::mCollisionAbort");
    static_assert(offsetof(AIActionHeliPursuit, mPlayerSpeed) == 0xA0, "AIActionHeliPursuit::mPlayerSpeed");
    static_assert(offsetof(AIActionHeliPursuit, mPursuitMode) == 0xA4, "AIActionHeliPursuit::mPursuitMode");

    struct PursuitState {
        float          searchRadius = 80.0f;
        float          searchSpeed = 15.0f;
        float          searchDirection = 1.0f;
        float          searchLapTurned = 0.0f;
        float          searchHeight = 5.0f;
        float          searchClock = 0.0f;
        float          lastPursuitTime = 0.0f;
        float          strikeClock = 0.0f;
        float          lastStrikeTime = 0.0f;
        float          strikeStart = 0.0f;
        bool           striking = false;
        bool           strikeHit = false;
        bool           sweeping = false;
        bool           approaching = false;
        bool           attacked = false;
        float          approachStart = 0.0f;
        int            pointsSinceHover = 0;
        bool           hoverHere = false;
        float          hoverStart = 0.0f;
        float          hoverEnd = -1.0f;
        float          hoverLookAngle = 0.0f;
        UMath::Vector3 hoverPoint = {};
        int            place = 0;
#if defined(_DEBUG)
        float          lastStrikeLog = 0.0f;
        int            loggedMode = -1;
#endif
    };

    struct FormationSpot {
        float side;
        float back;
        float up;
    };

    constexpr FormationSpot kFormationSpots[] = { { 22.0f, 10.0f, 5.0f }, { -22.0f, 10.0f, 5.0f }, { 0.0f, 60.0f, 10.0f } };
    constexpr int           kNumFormationSpots = static_cast<int>(sizeof(kFormationSpots) / sizeof(kFormationSpots[0]));
    constexpr float         kSearchSpread[] = { 0.0f, 0.15f, -0.15f, 0.5f };
    constexpr int           kNumSearchSpreads = static_cast<int>(sizeof(kSearchSpread) / sizeof(kSearchSpread[0]));

    class Squad {
      public:
        int Join(const AIActionHeliPursuit* action, float now) {
            Prune(now);
            const int place = Place(action);
            if (place >= 0) {
                mMembers[place].seen = now;
                return place;
            }
            if (mCount == kMostInSquad) return kMostInSquad - 1;
            if (mCount == 0) mLeadSince = now;
            mMembers[mCount++] = { action, now };
            Log::Info("AIActionHeliPursuit 0x%p  joined the squad at place %d of %d", action, mCount - 1, mCount);
            return mCount - 1;
        }

        int Place(const AIActionHeliPursuit* action) const {
            for (int i = 0; i < mCount; ++i)
                if (mMembers[i].action == action) return i;
            return -1;
        }

        void SendToBack(const AIActionHeliPursuit* action, float now) {
            const int place = Place(action);
            if (place < 0 || place == mCount - 1) return;
            const Member member = mMembers[place];
            for (int i = place; i + 1 < mCount; ++i) mMembers[i] = mMembers[i + 1];
            mMembers[mCount - 1] = member;
            if (place == 0) mLeadSince = now;
            Log::Info("AIActionHeliPursuit 0x%p  moved from place %d to the back of the squad, 0x%p leads now", action, place, mMembers[0].action);
        }

        bool MayAttack(const AIActionHeliPursuit* action, float now) const {
            if (mCount <= 1) return true;
            return mMembers[0].action == action && now >= mLeadSince && now - mLeadSince >= kTurnGap;
        }

        int Size() const {
            return mCount;
        }

      private:
        struct Member {
            const AIActionHeliPursuit* action;
            float                      seen;
        };

        void Prune(float now) {
            int kept = 0;
            for (int i = 0; i < mCount; ++i) {
                if (now >= mMembers[i].seen && now - mMembers[i].seen <= kSquadTimeout) {
                    mMembers[kept++] = mMembers[i];
                    continue;
                }
                if (kept == 0) mLeadSince = now;
                Log::Info("AIActionHeliPursuit 0x%p  left the squad from place %d", mMembers[i].action, i);
            }
            mCount = kept;
        }

        Member mMembers[kMostInSquad] = {};
        int    mCount = 0;
        float  mLeadSince = 0.0f;
    };

    AIActionHeliPursuit*                         gConstructed[kTrackedActions] = {};
    int                                          gNextConstructed = 0;
    PerHelicopter<PursuitState, kTrackedActions> gPursuits;
    PursuitState                                 gUnknownHelicopter;
    PursuitState*                                gHeli = &gUnknownHelicopter;
    Squad                                        gSquad;
    float                                        gEscapeHeadingX = 0.0f;
    float                                        gEscapeHeadingZ = 0.0f;
    bool                                         gPerpCovered = false;
    uintptr_t                                    gVisitedSpots[kMostVisitedSpots] = {};
    int                                          gNextVisitedSpot = 0;
    float                                        gLastSearchTime = -1.0f;
    uint32_t                                     gRandomState = 0x2545F491u;

    float SimTime() {
        return Game::Call<float>(kSim_GetTime);
    }

    int JoinSquad(const AIActionHeliPursuit* action, float now) {
        gHeli = &gPursuits.Use(action);
        gHeli->place = gSquad.Join(action, now);
        return gHeli->place;
    }

#if defined(_DEBUG)
    const char* PursuitModeName(int mode) {
        switch (mode) {
        case AIActionHeliPursuit::kStraight_Line: return "kStraight_Line";
        case AIActionHeliPursuit::kSearch_Pattern: return "kSearch_Pattern";
        case AIActionHeliPursuit::kSkid_Hit_Approach: return "kSkid_Hit_Approach";
        case AIActionHeliPursuit::kSkid_Hit_Strike: return "kSkid_Hit_Strike";
        default: return "-";
        }
    }
#endif

    float RandomUnit() {
        gRandomState ^= gRandomState << 13;
        gRandomState ^= gRandomState >> 17;
        gRandomState ^= gRandomState << 5;
        return static_cast<float>(gRandomState & 0xFFFFFFu) / 16777216.0f;
    }

    float RandomRange(float low, float high) {
        return low + (high - low) * RandomUnit();
    }

    float Lerp(float from, float to, float t) {
        return from + (to - from) * t;
    }

    float WrapTurn(float turn) {
        turn -= std::floor(turn);
        return turn;
    }

    float SearchProgress() {
        const float span = AIActionHeliPursuit::sSettings.SearchRadiusLargest - AIActionHeliPursuit::sSettings.SearchRadiusFirst;
        if (span <= 0.0f) return 1.0f;
        return std::clamp((gHeli->searchRadius - AIActionHeliPursuit::sSettings.SearchRadiusFirst) / span, 0.0f, 1.0f);
    }

    void AdvanceSearchClock(float pursuitTime) {
        float dT = pursuitTime - gHeli->lastPursuitTime;
        if (dT < 0.0f) dT = pursuitTime;
        if (dT < 0.0f || dT > kLongestSearchStep) dT = 0.0f;
        gHeli->searchClock += dT;
        gHeli->lastPursuitTime = pursuitTime;
    }

    float Dot(const UMath::Vector3& a, const UMath::Vector3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    void AdvanceStrikeClock(float pursuitTime) {
        float dT = pursuitTime - gHeli->lastStrikeTime;
        if (dT < 0.0f) dT = pursuitTime;
        if (dT < 0.0f || dT > kLongestSearchStep) dT = 0.0f;
        gHeli->strikeClock += dT;
        gHeli->lastStrikeTime = pursuitTime;
    }

    bool WasVisited(uintptr_t zone) {
        for (uintptr_t visited : gVisitedSpots)
            if (visited == zone) return true;
        return false;
    }

    void MarkVisited(uintptr_t zone) {
        gVisitedSpots[gNextVisitedSpot] = zone;
        gNextVisitedSpot = (gNextVisitedSpot + 1) % kMostVisitedSpots;
    }

    bool FindHidingSpot(const UMath::Vector3& lastKnown, float radius, const UMath::Vector3& myPosition, UMath::Vector3& spot) {
        uintptr_t best = 0;
        float     bestDistance = 0.0f;
        for (eTrackPathZoneType type : kHidingZoneTypes) {
            const uintptr_t info = kTheTrackPathManager + kZoneInfoTable + kZoneInfoSize * static_cast<unsigned>(type);
            int       numZones = 0;
            uintptr_t zone = 0;
            uintptr_t last = 0;
            if (!Memory::Read(info + kZoneInfoNumZones, &numZones, sizeof(numZones)) || numZones <= 0
                || !Memory::Read(info + kZoneInfoFirstZone, &zone, sizeof(zone)) || !Memory::Read(info + kZoneInfoLastZone, &last, sizeof(last)))
                continue;
            for (int scanned = 0; zone != 0 && zone < last && scanned < kMostZonesScanned; ++scanned) {
                int     zoneType = 0;
                float   box[4] = {};
                float   elevation = 0.0f;
                int16_t size = 0;
                if (!Memory::Read(zone, &zoneType, sizeof(zoneType)) || !Memory::Read(zone + kZoneBBoxMin, box, sizeof(box))
                    || !Memory::Read(zone + kZoneElevation, &elevation, sizeof(elevation))
                    || !Memory::Read(zone + kZoneMemoryImageSize, &size, sizeof(size)) || size <= 0)
                    break;
                if (zoneType != type) {
                    zone += static_cast<uintptr_t>(size);
                    continue;
                }
                const UMath::Vector3 centre = { -(box[1] + box[3]) * 0.5f, elevation != 0.0f ? elevation : lastKnown.y, (box[0] + box[2]) * 0.5f };
                const float distance = UMath::Distancexz(centre, myPosition);
                if (UMath::Distancexz(centre, lastKnown) <= radius && !WasVisited(zone) && (best == 0 || distance < bestDistance)) {
                    best = zone;
                    bestDistance = distance;
                    spot = centre;
                }
                zone += static_cast<uintptr_t>(size);
            }
        }
        if (best == 0) return false;
        MarkVisited(best);
        return true;
    }

    bool GroundUnitsDirection(IPursuit* ip, const IVehicle* heli, const UMath::Vector3& lastKnown, float& x, float& z) {
        uintptr_t vehicles = 0;
        int       count = 0;
        const uintptr_t pursuit = reinterpret_cast<uintptr_t>(ip);
        if (!Memory::Read(pursuit + kPursuitVehicles, &vehicles, sizeof(vehicles)) || !Memory::Read(pursuit + kPursuitNumVehicles, &count, sizeof(count))
            || vehicles == 0 || count <= 0 || count > kMostGroundUnits)
            return false;
        float sumX = 0.0f;
        float sumZ = 0.0f;
        int   used = 0;
        for (int i = 0; i < count; ++i) {
            uintptr_t vehicle = 0;
            if (!Memory::Read(vehicles + i * sizeof(uintptr_t), &vehicle, sizeof(vehicle)) || vehicle == 0
                || vehicle == reinterpret_cast<uintptr_t>(heli))
                continue;
            const UMath::Vector3* position =
                Game::ThisCall<const UMath::Vector3*>(Game::VirtualFunction(reinterpret_cast<const void*>(vehicle), kIVehicleGetPosition), vehicle);
            if (position == nullptr || UMath::Distancexz(*position, lastKnown) > kGroundUnitRange) continue;
            sumX += position->x - lastKnown.x;
            sumZ += position->z - lastKnown.z;
            ++used;
        }
        const float length = std::sqrt(sumX * sumX + sumZ * sumZ);
        if (used == 0 || length < 1.0f) return false;
        x = sumX / length;
        z = sumZ / length;
        return true;
    }

    bool IsAlive(const AIActionHeliPursuit* action, const IRigidBody* heliRigidBody) {
        uintptr_t vtable = 0;
        const IRigidBody* rigidBody = nullptr;
        const uintptr_t va = reinterpret_cast<uintptr_t>(action);
        return action && Memory::Read(va, &vtable, sizeof(vtable)) && vtable == kVTable
            && Memory::Read(va + offsetof(AIActionHeliPursuit, mIRigidBody), &rigidBody, sizeof(rigidBody)) && rigidBody == heliRigidBody;
    }

}

void AIActionHeliPursuit::Refresh() {
    if (sSettings.LeadBase > sSettings.LeadMax)
        Log::Warn("[Helicopter:Leading] LeadBase=%g > LeadMax=%g  leadDist pinned at %g", sSettings.LeadBase, sSettings.LeadMax, sSettings.LeadMax);

    if (sSettings.SearchSpeedMin > sSettings.SearchSpeedMax) {
        Log::Warn("[Helicopter:SearchSpeed] SearchSpeedMin=%g > SearchSpeedMax=%g  -> %g", sSettings.SearchSpeedMin, sSettings.SearchSpeedMax,
                  sSettings.SearchSpeedMax);
        sSettings.SearchSpeedMin = sSettings.SearchSpeedMax;
    }
    if (sSettings.SearchRadiusFirst > sSettings.SearchRadiusLargest) {
        Log::Warn("[Helicopter:SearchRadius] SearchRadiusFirst=%g > SearchRadiusLargest=%g  -> %g", sSettings.SearchRadiusFirst,
                  sSettings.SearchRadiusLargest, sSettings.SearchRadiusLargest);
        sSettings.SearchRadiusFirst = sSettings.SearchRadiusLargest;
    }

    sLeadDistance.base = sSettings.LeadBase;
    sLeadDistance.max  = sSettings.LeadMax;
    sClosingSpeed.nearSpeed = sSettings.ClosingSpeedNear;
    sClosingSpeed.midSpeed  = sSettings.ClosingSpeedMid;
    sClosingSpeed.farSpeed  = sSettings.ClosingSpeedFar;
    Log::Info("AIActionHeliPursuit  sClosingSpeed@0x%p near=%g mid=%g far=%g", &sClosingSpeed, sClosingSpeed.nearSpeed, sClosingSpeed.midSpeed,
              sClosingSpeed.farSpeed);

    if (sSkidKnockPatched && sSettings.AttackDuration != sSkidKnockTime) {
        if (Patch::RewriteStoredFloat("AIActionHeliPursuit::StraightLinePursuit | mSkidKnockTimer = kSkidKnockTime", kStraightLineSkidKnockTime,
                                      sSkidKnockTime, sSettings.AttackDuration))
            sSkidKnockTime = sSettings.AttackDuration;
        else
            sSkidKnockPatched = false;
    }
    Log::Info("AIActionHeliPursuit  sLeadDistance@0x%p base=%g max=%g", &sLeadDistance, sLeadDistance.base, sLeadDistance.max);
}

void __cdecl AIActionHeliPursuit::ConstructorEntry(Detour::Registers* registers) {
    auto* action = reinterpret_cast<AIActionHeliPursuit*>(static_cast<uintptr_t>(registers->ecx));
    if (!action) return;
    gPursuits.Forget(action);
    for (const AIActionHeliPursuit* known : gConstructed)
        if (known == action) return;
    gConstructed[gNextConstructed] = action;
    Log::Info("AIActionHeliPursuit::AIActionHeliPursuit  ecx=0x%08X gConstructed[%d]", registers->ecx, gNextConstructed);
    gNextConstructed = (gNextConstructed + 1) % kTrackedActions;
}

AIActionHeliPursuit* AIActionHeliPursuit::Find(const IRigidBody* heliRigidBody) {
    for (AIActionHeliPursuit* action : gConstructed)
        if (IsAlive(action, heliRigidBody)) return action;
    return nullptr;
}

void AIActionHeliPursuit::StraightLinePursuit() {
    Game::ThisCall<void>(kStraightLinePursuit, this);
}

void AIActionHeliPursuit::SkidHitPursuit() {
    Game::ThisCall<void>(kSkidHitPursuit, this);
}

bool AIActionHeliPursuit::CanKeepDiving() const {
    if (mPursuitMode != kSkid_Hit_Strike || gHeli->strikeHit || mPlayerRigidBody == nullptr || mIRigidBody == nullptr) return false;
    if (gHeli->strikeClock - gHeli->strikeStart >= kLongestStrike) return false;
    const UMath::Vector3 heli = mIRigidBody->GetPosition();
    const UMath::Vector3 car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3 apart = heli - car;
    return apart.y > kUnderCar && apart.x * apart.x + apart.y * apart.y + apart.z * apart.z <= kStrikeReach * kStrikeReach;
}

bool AIActionHeliPursuit::IsPerpReversing() const {
    if (mPlayerRigidBody == nullptr) return false;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    if (perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z < kReverseSpeed * kReverseSpeed) return false;
    UMath::Vector3 forward = {};
    mPlayerRigidBody->GetForwardVector(forward);
    return perpLinVel.x * forward.x + perpLinVel.z * forward.z < 0.0f;
}

bool AIActionHeliPursuit::KeepsUpWhileReversing() const {
    if (mIRigidBody == nullptr || !IsPerpReversing()) return false;
    const UMath::Vector3  car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    const UMath::Vector3  heli = mIRigidBody->GetPosition();
    const UMath::Vector3  apart = heli - car;
    const float distanceSq = apart.x * apart.x + apart.y * apart.y + apart.z * apart.z;
    if (apart.x * perpLinVel.x + apart.z * perpLinVel.z > 0.0f) return distanceSq <= kAheadReachSq && heli.y >= car.y;
    return distanceSq <= kBehindReachSq;
}

bool AIActionHeliPursuit::ShouldStrikeNow() const {
    if (mPlayerRigidBody == nullptr || mIRigidBody == nullptr) return false;
    const UMath::Vector3  car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    const UMath::Vector3  heli = mIRigidBody->GetPosition();
    UMath::Vector3 right = {};
    mPlayerRigidBody->GetRightVector(right);
    const UMath::Vector3 apart = heli - car;
    if (std::fabs(apart.x * right.x + apart.y * right.y + apart.z * right.z) <= kSideClearance) return false;

    const UMath::Vector3 lineUp = car + carVelocity * kApproachLead + mSkidHitOffset;
    const float speed = std::sqrt(carVelocity.x * carVelocity.x + carVelocity.z * carVelocity.z);
    if (UMath::Distancexz(heli, lineUp) <= std::max(kGameStrikeStart, speed * kStrikeStartPerSpeed)) return true;
    return gHeli->strikeClock - gHeli->approachStart >= kApproachPatience && apart.x * apart.x + apart.y * apart.y + apart.z * apart.z <= kStrikeReach * kStrikeReach;
}

void AIActionHeliPursuit::BeginStrike() {
    Log::Info("AIActionHeliPursuit 0x%p  strike started by the mod %.2fs into the approach  mSkidKnockTimer[+0x64]=%.2f", this,
              gHeli->strikeClock - gHeli->approachStart, mSkidKnockTimer);
    mPursuitMode = kSkid_Hit_Strike;
    if (mSkidKnockTimer < kGameStrikeTime) mSkidKnockTimer = kGameStrikeTime;
    gHeli->approaching = false;
    void* soundAI = Game::Global<void*>(kSoundAI_mInstance);
    void* speaker = soundAI != nullptr ? *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(soundAI) + kSoundAIHeli) : nullptr;
    if (speaker != nullptr) Game::ThisCall<void>(Game::VirtualFunction(speaker, kIntentToRam), speaker);
}

bool AIActionHeliPursuit::IsPerpUnderCover() const {
    if (mPlayerRigidBody == nullptr) return false;
    const UMath::Vector3  car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    const bool covered = HeliSheet::UnderCover(car) || HeliSheet::UnderCover(car + carVelocity * kCoverLookAhead);
    if (covered != gPerpCovered) {
        gPerpCovered = covered;
        Log::Info("AIActionHeliPursuit 0x%p  perp %s a tunnel/overpass zone at (%.1f, %.1f, %.1f), new skid hits %s", this, covered ? "under" : "out of", car.x,
                  car.y, car.z, covered ? "held back" : "allowed");
    }
    return covered;
}

void AIActionHeliPursuit::StrikePerp() {
    JoinSquad(this, SimTime());
    gHeli->attacked = true;
    AdvanceStrikeClock(mPursuitTime);
    if (mPursuitMode != kSkid_Hit_Strike) gHeli->striking = false;
    const bool keepDiving = gHeli->striking && CanKeepDiving();
    if (keepDiving && mSkidKnockTimer < kStrikeHold) mSkidKnockTimer = kStrikeHold;
    const kPursuitMode modeBefore = mPursuitMode;
    const float timerBefore = mSkidKnockTimer;

    SkidHitPursuit();

    if (mPursuitMode == kStraight_Line && timerBefore > 0.0f && KeepsUpWhileReversing()) {
        Log::Info("AIActionHeliPursuit 0x%p  kept the attack going while the perp reverses (the game judged it by the car's nose)  mSkidKnockTimer[+0x64]=%.2f",
                  this, timerBefore);
        mPursuitMode = modeBefore;
        mSkidKnockTimer = timerBefore;
    }

    if (mPursuitMode != kSkid_Hit_Approach) {
        gHeli->approaching = false;
    } else {
        if (!gHeli->approaching) {
            gHeli->approaching = true;
            gHeli->approachStart = gHeli->strikeClock;
        }
        if (ShouldStrikeNow()) BeginStrike();
    }

    if (keepDiving && mPursuitMode != kSkid_Hit_Strike) {
        Log::Info("AIActionHeliPursuit 0x%p  strike kept going %.2fs in: the game gave up with no hit (mSkidKnockTimer[+0x64]=%.2f)", this,
                  gHeli->strikeClock - gHeli->strikeStart, mSkidKnockTimer);
        mPursuitMode = kSkid_Hit_Strike;
        mSkidKnockTimer = kStrikeHold;
    }
    if (mPursuitMode != kSkid_Hit_Strike || mPlayerRigidBody == nullptr || mIRigidBody == nullptr) {
        gHeli->striking = false;
        return;
    }
    if (!gHeli->striking) {
        gHeli->striking = true;
        gHeli->strikeHit = false;
        gHeli->sweeping = false;
        gHeli->strikeStart = gHeli->strikeClock;
        Log::Info("AIActionHeliPursuit 0x%p  strike begins  mSkidKnockTimer[+0x64]=%.2f mSkidHitOffset[+0x90]=(%.2f, %.2f, %.2f)", this, mSkidKnockTimer,
                  mSkidHitOffset.x, mSkidHitOffset.y, mSkidHitOffset.z);
    }
    Dive();
}

void AIActionHeliPursuit::BounceOff(const UMath::Vector3& outward) {
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    const UMath::Vector3& heliVelocity = mIRigidBody->GetLinearVelocity();
    const float closing = -Dot(heliVelocity - carVelocity, outward);
    if (closing <= 0.0f) return;

    const UMath::Vector3 velocity = heliVelocity + outward * closing;
    mIRigidBody->SetLinearVelocity(velocity);
    Log::Info("AIActionHeliPursuit 0x%p  strike bounced off the car, stopped %.1f m/s of closing speed", this, closing);
}

void AIActionHeliPursuit::Dive() {
    const UMath::Vector3  car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    const UMath::Vector3  heli = mIRigidBody->GetPosition();
    const UMath::Vector3& heliVelocity = mIRigidBody->GetLinearVelocity();
    const UMath::Vector3  apart = heli - car;
    const float           heliSpeed = std::sqrt(heliVelocity.x * heliVelocity.x + heliVelocity.y * heliVelocity.y + heliVelocity.z * heliVelocity.z);

    UMath::Vector3 forward = {};
    UMath::Vector3 right = {};
    mPlayerRigidBody->GetForwardVector(forward);
    mPlayerRigidBody->GetRightVector(right);
    const float          ahead = Dot(apart, forward);
    const float          side = Dot(apart, right);
    UMath::Vector3 carSize = {};
    mPlayerRigidBody->GetDimension(carSize);
    const UMath::Vector3 outward = right * (side < 0.0f ? -1.0f : 1.0f);

    UMath::Vector3 heliForward = {};
    UMath::Vector3 heliRight = {};
    UMath::Vector3 heliUp = {};
    mIRigidBody->GetForwardVector(heliForward);
    mIRigidBody->GetRightVector(heliRight);
    mIRigidBody->GetUpVector(heliUp);
    const UMath::Vector3& cabinOffset = CollisionGeometry::Collections::kHeliCabinOffset;
    const UMath::Vector3& heliSize = CollisionGeometry::Collections::kHeliCabinHalf;
    const UMath::Vector3  cabin = heli + heliRight * cabinOffset.x + heliUp * cabinOffset.y + heliForward * cabinOffset.z - car;
    const bool touching = std::fabs(Dot(cabin, right)) <= carSize.x + heliSize.x - kContactOverlap && std::fabs(Dot(cabin, forward)) <= carSize.z + heliSize.z &&
                          std::fabs(cabin.y) <= carSize.y + heliSize.y;
    if (!gHeli->strikeHit && touching) {
        gHeli->strikeHit = true;
        if (mSkidKnockTimer > kAfterHit) mSkidKnockTimer = kAfterHit;
        Log::Info("AIActionHeliPursuit 0x%p  strike hit the car %.2fs in  ahead=%.2f side=%.2f up=%.2f heliSpeed=%.1f  car half=(%.2f %.2f %.2f) heli half=(%.2f %.2f %.2f)",
                  this, gHeli->strikeClock - gHeli->strikeStart, ahead, side, apart.y, heliSpeed, carSize.x, carSize.y, carSize.z, heliSize.x, heliSize.y, heliSize.z);
        BounceOff(outward);
    }
    if (!gHeli->sweeping && apart.y <= kSweepHeight) {
        gHeli->sweeping = true;
        Log::Info("AIActionHeliPursuit 0x%p  strike level with the car %.2fs in, sweeping across  dXZ=%.2f dY=%.2f", this, gHeli->strikeClock - gHeli->strikeStart,
                  UMath::Distancexz(heli, car), apart.y);
    }

    UMath::Vector3 target = car + carVelocity * kStrikeLead;
    if (gHeli->strikeHit) {
        target = target + outward * kBounceSide;
        target.y = car.y + kBounceHeight;
    } else {
        target = target + mSkidHitOffset * (gHeli->sweeping ? -kStrikeCrossing : kLineUpDistance / kSkidHitSide);
        target.y = car.y + kStrikeHeight;
    }
    mIVehicleAI->SetDriveTarget(target);

#if defined(_DEBUG)
    if (gHeli->strikeClock - gHeli->lastStrikeLog < 0.25f && gHeli->strikeClock >= gHeli->lastStrikeLog) return;
    gHeli->lastStrikeLog = gHeli->strikeClock;
    const UMath::Vector3 aim = target - car;
    Log::Info("AIActionHeliPursuit 0x%p  strike %.2fs %s  heli ahead=%.1f side=%.1f up=%.1f speed=%.1f  target ahead=%.1f side=%.1f up=%.1f  "
              "mSkidKnockTimer[+0x64]=%.2f",
              this, gHeli->strikeClock - gHeli->strikeStart, gHeli->strikeHit ? "bounce" : gHeli->sweeping ? "sweep" : "beside", ahead, side, apart.y, heliSpeed,
              Dot(aim, forward), Dot(aim, right), aim.y, mSkidKnockTimer);
#endif
}

void AIActionHeliPursuit::LeadReversing(const UMath::Vector3& perpLinVel, float speed) {
    const UMath::Vector3 player = mPlayerRigidBody->GetPosition();
    UMath::Vector3 seek = mIVehicleAI->GetDriveTarget();
    const float lead = UMath::Distancexz(seek, player);
    seek.x = player.x + perpLinVel.x / speed * lead;
    seek.z = player.z + perpLinVel.z / speed * lead;
    ChaseToward(seek, perpLinVel);
    mIAIHelicopter->SetLookAtPosition(seek);
}

void AIActionHeliPursuit::HoldFormationSpot(int place, const UMath::Vector3& perpLinVel) {
    if (place <= 0) return;
    const FormationSpot& spot = kFormationSpots[std::min(place, kNumFormationSpots) - 1];
    const UMath::Vector3 player = mPlayerRigidBody->GetPosition();
    UMath::Vector3 seek = mIVehicleAI->GetDriveTarget();
    float aheadX = seek.x - player.x;
    float aheadZ = seek.z - player.z;
    const float lead = std::sqrt(aheadX * aheadX + aheadZ * aheadZ);
    if (lead < kShortestLead) return;
    aheadX /= lead;
    aheadZ /= lead;
    seek.x += aheadZ * spot.side - aheadX * spot.back;
    seek.z -= aheadX * spot.side + aheadZ * spot.back;
    seek.y += spot.up;
    ChaseToward(seek, perpLinVel);
}

void AIActionHeliPursuit::ChaseToward(const UMath::Vector3& seek, const UMath::Vector3& perpLinVel) {
    const UMath::Vector3 me = mIRigidBody->GetPosition();
    const float toSeekX = seek.x - me.x;
    const float toSeekZ = seek.z - me.z;
    const float distance = std::sqrt(toSeekX * toSeekX + toSeekZ * toSeekZ);
    float adjust = distance > 0.0f ? (toSeekX * perpLinVel.x + toSeekZ * perpLinVel.z) / distance : 0.0f;
    if (adjust < 0.0f) adjust *= 0.5f;
    const float closing = distance > kFarSeek ? sClosingSpeed.farSpeed
                        : distance > kMidSeek ? sClosingSpeed.midSpeed
                        : distance > kNearSeek ? sClosingSpeed.nearSpeed
                                               : distance;

    mIVehicleAI->SetDriveSpeed(adjust + closing);
    mIVehicleAI->SetDriveTarget(seek);
}

void AIActionHeliPursuit::ChasePerp() {
    const float now = SimTime();
    const int joined = JoinSquad(this, now);
    if (gHeli->attacked) {
        gHeli->attacked = false;
        gSquad.SendToBack(this, now);
    } else if (joined == 0 && gSquad.Size() > 1 && mSkidKnockTimer < -kLeadPatience) {
        Log::Info("AIActionHeliPursuit 0x%p  no attack chance for %.1fs as squad lead, handing over  mSkidKnockTimer[+0x64]=%.2f", this,
                  kSkidHitNotYet - mSkidKnockTimer, mSkidKnockTimer);
        gSquad.SendToBack(this, now);
    }
    const int place = gHeli->place = gSquad.Place(this);
    const bool covered = IsPerpUnderCover();
    if ((covered || !gSquad.MayAttack(this, now)) && mSkidKnockTimer < kSkidHitNotYet) mSkidKnockTimer = kSkidHitNotYet;
    StraightLinePursuit();
    if (IsSkidHitting()) gHeli->attacked = true;
    if (mPlayerRigidBody == nullptr) return;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    const float speed = std::sqrt(perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z);
    if (mPursuitMode == kStraight_Line && IsPerpReversing()) LeadReversing(perpLinVel, speed);
    if (mPursuitMode == kStraight_Line) HoldFormationSpot(place, perpLinVel);
    if (speed < kEscapeHeadingSpeed) return;
    gEscapeHeadingX = perpLinVel.x / speed;
    gEscapeHeadingZ = perpLinVel.z / speed;
}

void AIActionHeliPursuit::StartSearch() {
    const float now = SimTime();
    JoinSquad(this, now);
    if (now < gLastSearchTime || now - gLastSearchTime > kSquadTimeout) {
        for (uintptr_t& visited : gVisitedSpots) visited = 0;
        gNextVisitedSpot = 0;
    }
    gLastSearchTime = now;
    gHeli->searchRadius = sSettings.SearchRadiusFirst - kSearchRadiusGrowth;
    gHeli->searchLapTurned = 0.0f;
    gHeli->searchDirection = RandomUnit() < 0.5f ? 1.0f : -1.0f;
    gHeli->pointsSinceHover = 0;
    gHeli->hoverHere = false;
    gHeli->hoverEnd = -1.0f;
    gHeli->lastPursuitTime = mPursuitTime;
    Log::Info("AIActionHeliPursuit 0x%p  StartSearch  mPursuitTime[+0x60]=%.2f dir=%+.0f escape=(%.3f, %.3f) mSearchPatternAngle[+0x70]=%.3f place=%d", this,
              mPursuitTime, gHeli->searchDirection, gEscapeHeadingX, gEscapeHeadingZ, mSearchPatternAngle, gHeli->place);
    SetNextSearchPoint();
}

void AIActionHeliPursuit::SetNextSearchPoint() {
    IPursuit* ip = mIVehicleAI->GetPursuit();
    if (ip == nullptr) return;

    const UMath::Vector3& lastKnown = ip->GetLastKnownLocation();
    const UMath::Vector3  myPosition = mIRigidBody->GetPosition();

    gHeli->searchRadius = std::min(gHeli->searchRadius + kSearchRadiusGrowth, sSettings.SearchRadiusLargest);
    const float progress = SearchProgress();
    gHeli->searchSpeed  = Lerp(sSettings.SearchSpeedMin, sSettings.SearchSpeedMax, progress);
    gHeli->searchHeight = Lerp(kSearchHeightNear, kSearchHeightFar, progress);
    gHeli->hoverHere    = ++gHeli->pointsSinceHover >= kPointsPerHover;

    UMath::Vector3 spot = {};
    if (gHeli->hoverHere && FindHidingSpot(lastKnown, gHeli->searchRadius, myPosition, spot)) {
        mSearchDestPoint = spot;
        Log::Info("AIActionHeliPursuit 0x%p  SetNextSearchPoint  hiding spot (%.2f, %.2f, %.2f) radius=%.1f", this, spot.x, spot.y, spot.z,
                  gHeli->searchRadius);
    } else {
        const bool  haveHeading = gEscapeHeadingX != 0.0f || gEscapeHeadingZ != 0.0f;
        const float headingTurn = haveHeading ? std::atan2(gEscapeHeadingZ, gEscapeHeadingX) / kTwoPi : 0.0f;
        float       offset = 0.0f;
        if (haveHeading) {
            gHeli->searchDirection = -gHeli->searchDirection;
            offset = gHeli->searchDirection * RandomRange(0.0f, Lerp(kSearchConeFirst, kSearchConeLargest, progress));
            mSearchPatternAngle = WrapTurn(headingTurn + offset);
        } else {
            const float step = RandomRange(kSearchSmallestStep, kSearchLargestStep);
            gHeli->searchLapTurned += step;
            if (gHeli->searchLapTurned >= 1.0f) {
                gHeli->searchLapTurned -= 1.0f;
                if (RandomUnit() < 0.5f) gHeli->searchDirection = -gHeli->searchDirection;
            }
            mSearchPatternAngle = WrapTurn(mSearchPatternAngle + step * gHeli->searchDirection);
        }

        float groundX = 0.0f;
        float groundZ = 0.0f;
        if (GroundUnitsDirection(ip, mIVehicle, lastKnown, groundX, groundZ)) {
            const float angle = mSearchPatternAngle * kTwoPi;
            if (std::cos(angle) * groundX + std::sin(angle) * groundZ > kGroundUnitSide)
                mSearchPatternAngle = WrapTurn(haveHeading ? headingTurn - offset : mSearchPatternAngle + 0.5f);
        }

        const float radius = gHeli->searchRadius * RandomRange(1.0f - kSearchRadiusJitter, 1.0f + kSearchRadiusJitter);
        const float angle = (mSearchPatternAngle + kSearchSpread[std::clamp(gHeli->place, 0, kNumSearchSpreads - 1)]) * kTwoPi;
        mSearchDestPoint = { lastKnown.x + std::cos(angle) * radius, lastKnown.y, lastKnown.z + std::sin(angle) * radius };
        mIAIHelicopter->RestrictPointToRoadNet(mSearchDestPoint);
    }
    mIAIHelicopter->FilterHeliAltitude(mSearchDestPoint);
    mSearchDestPoint.y += gHeli->searchHeight;
    Log::Info("AIActionHeliPursuit 0x%p  SetNextSearchPoint  GetLastKnownLocation=(%.2f, %.2f, %.2f) radius=%.1f speed=%.1f height=%.1f hover=%d "
              "mSearchPatternAngle[+0x70]=%.3f mSearchDestPoint[+0x74]=(%.2f, %.2f, %.2f)",
              this, lastKnown.x, lastKnown.y, lastKnown.z, gHeli->searchRadius, gHeli->searchSpeed, gHeli->searchHeight, gHeli->hoverHere, mSearchPatternAngle,
              mSearchDestPoint.x, mSearchDestPoint.y, mSearchDestPoint.z);
}

void AIActionHeliPursuit::SearchForPerp() {
    gLastSearchTime = SimTime();
    JoinSquad(this, gLastSearchTime);
    AdvanceSearchClock(mPursuitTime);
    const UMath::Vector3 myPosition = mIRigidBody->GetPosition();

    if (gHeli->hoverEnd >= 0.0f) {
        if (gHeli->searchClock < gHeli->hoverEnd) {
            HoverAndLook(myPosition);
            return;
        }
        gHeli->hoverEnd = -1.0f;
        SetNextSearchPoint();
    } else if (UMath::Distancexz(myPosition, mSearchDestPoint) < kSearchNextPointRange) {
        if (gHeli->hoverHere) {
            gHeli->hoverHere = false;
            gHeli->pointsSinceHover = 0;
            gHeli->hoverPoint = myPosition;
            gHeli->hoverStart = gHeli->searchClock;
            gHeli->hoverEnd = gHeli->searchClock + RandomRange(kHoverShortest, kHoverLongest);
            gHeli->hoverLookAngle = mSearchPatternAngle;
            HoverAndLook(myPosition);
            return;
        }
        SetNextSearchPoint();
    }

    mIVehicleAI->SetDriveSpeed(gHeli->searchSpeed);
    mIAIHelicopter->SetDestinationVelocity(mIRigidBody->GetLinearVelocity());
    mIVehicleAI->SetDriveTarget(mSearchDestPoint);
    mIAIHelicopter->SetLookAtPosition(SearchLookAt(myPosition));
    mIVehicleAI->DoDriving(kSearchDrivingFlags);
}

void AIActionHeliPursuit::HoverAndLook(const UMath::Vector3& myPosition) {
    const float duration = gHeli->hoverEnd - gHeli->hoverStart;
    const float turned = duration > 0.0f ? (gHeli->searchClock - gHeli->hoverStart) / duration : 1.0f;
    const float angle = (gHeli->hoverLookAngle + turned) * kTwoPi;
    const UMath::Vector3 lookAt = { myPosition.x + std::cos(angle) * kLookDistance, gHeli->hoverPoint.y - kHoverLookDrop,
                                    myPosition.z + std::sin(angle) * kLookDistance };
    const UMath::Vector3 still = {};

    mIVehicleAI->SetDriveSpeed(0.0f);
    mIAIHelicopter->SetDestinationVelocity(still);
    mIVehicleAI->SetDriveTarget(gHeli->hoverPoint);
    mIAIHelicopter->SetLookAtPosition(lookAt);
    mIVehicleAI->DoDriving(kSearchDrivingFlags);
}

UMath::Vector3 AIActionHeliPursuit::SearchLookAt(const UMath::Vector3& myPosition) const {
    const float distance = UMath::Distancexz(myPosition, mSearchDestPoint);
    if (distance < 0.1f) return mSearchDestPoint;

    const float forwardX = (mSearchDestPoint.x - myPosition.x) / distance;
    const float forwardZ = (mSearchDestPoint.z - myPosition.z) / distance;
    const float sweep = std::sin(mPursuitTime * kTwoPi / kLookSweepPeriod) * kLookSweepAngle;
    const float c = std::cos(sweep);
    const float s = std::sin(sweep);
    return { myPosition.x + (forwardX * c - forwardZ * s) * kLookDistance, mSearchDestPoint.y,
             myPosition.z + (forwardX * s + forwardZ * c) * kLookDistance };
}

#if defined(_DEBUG)
void AIActionHeliPursuit::LogLive(bool full) const {
    gHeli = &gPursuits.Use(this);
    if (mPursuitMode != gHeli->loggedMode) {
        Log::Info("AIActionHeliPursuit 0x%p  mPursuitMode[+0xA4] %d %s -> %d %s  mPursuitTime[+0x60]=%.2f mSkidKnockTimer[+0x64]=%.2f place=%d", this,
                  gHeli->loggedMode, PursuitModeName(gHeli->loggedMode), mPursuitMode, PursuitModeName(mPursuitMode), mPursuitTime, mSkidKnockTimer,
                  gSquad.Place(this));
        gHeli->loggedMode = mPursuitMode;
    }
    if (!full) return;

    Log::Info("AIActionHeliPursuit 0x%p  vtbl=0x%08X mPursuitMode[+0xA4]=%d %s mPursuitTime[+0x60]=%.2f mSkidKnockTimer[+0x64]=%.2f "
              "mPathTime[+0x68]=%.2f mBuildingPath[+0x6C]=%d mCollisionAbort[+0x9C]=%d",
              this, *reinterpret_cast<const uint32_t*>(this), mPursuitMode, PursuitModeName(mPursuitMode), mPursuitTime, mSkidKnockTimer, mPathTime,
              mBuildingPath, mCollisionAbort);
    Log::Info("  mIVehicleAI[+0x4C]=0x%p mIRigidBody[+0x54]=0x%p mIAIHelicopter[+0x58]=0x%p mPlayerRigidBody[+0x80]=0x%p", mIVehicleAI, mIRigidBody,
              mIAIHelicopter, mPlayerRigidBody);
    Log::Info("  mPlayerPosition[+0x84]=(%.2f, %.2f, %.2f) mPlayerSpeed[+0xA0]=%.2f mSkidHitOffset[+0x90]=(%.2f, %.2f, %.2f)", mPlayerPosition.x,
              mPlayerPosition.y, mPlayerPosition.z, mPlayerSpeed, mSkidHitOffset.x, mSkidHitOffset.y, mSkidHitOffset.z);
    Log::Info("  mSearchPatternAngle[+0x70]=%.3f mSearchDestPoint[+0x74]=(%.2f, %.2f, %.2f)  radius=%.1f speed=%.1f dir=%+.0f lap=%.3f "
              "escape=(%.3f, %.3f)",
              mSearchPatternAngle, mSearchDestPoint.x, mSearchDestPoint.y, mSearchDestPoint.z, gHeli->searchRadius, gHeli->searchSpeed, gHeli->searchDirection,
              gHeli->searchLapTurned, gEscapeHeadingX, gEscapeHeadingZ);

    IPursuit* ip = mIVehicleAI != nullptr ? mIVehicleAI->GetPursuit() : nullptr;
    if (ip != nullptr) {
        const UMath::Vector3& lastKnown = ip->GetLastKnownLocation();
        Log::Info("  IPursuit=0x%p GetPursuitDuration=%.2f GetEvadeLevel=%.3f GetCoolDownTimeRemaining=%.2f/%.2f IsPerpInSight=%d "
                  "GetNumHeliSpawns=%d GetLastKnownLocation=(%.2f, %.2f, %.2f)",
                  ip, ip->GetPursuitDuration(), ip->GetEvadeLevel(), ip->GetCoolDownTimeRemaining(), ip->GetCoolDownTimeRequired(), ip->IsPerpInSight(),
                  ip->GetNumHeliSpawns(), lastKnown.x, lastKnown.y, lastKnown.z);
    }
    Log::Info("  sLeadDistance@0x%p base=%g max=%g", &sLeadDistance, sLeadDistance.base, sLeadDistance.max);
}
#endif
