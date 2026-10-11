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
    constexpr float    kLookSweepAngle       = 0.79f;
    constexpr float    kLookSweepPeriod      = 10.0f;
    constexpr float    kLookDistance         = 60.0f;
    constexpr unsigned kSearchDrivingFlags   = 7u;

    constexpr int kTrackedActions = 4;

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

    AIActionHeliPursuit* gConstructed[kTrackedActions] = {};
    int                  gNextConstructed = 0;
    AIActionHeliPursuit* gCurrent = nullptr;
    float                gSearchRadius = 80.0f;
    float                gSearchSpeed = 15.0f;
    float                gSearchDirection = 1.0f;
    float                gSearchLapTurned = 0.0f;
    float                gEscapeHeadingX = 0.0f;
    float                gEscapeHeadingZ = 0.0f;
    float                gSearchHeight = 5.0f;
    float                gSearchClock = 0.0f;
    float                gLastPursuitTime = 0.0f;
    int                  gPointsSinceHover = 0;
    bool                 gHoverHere = false;
    float                gHoverStart = 0.0f;
    float                gHoverEnd = -1.0f;
    float                gHoverLookAngle = 0.0f;
    UMath::Vector3       gHoverPoint = {};
    uintptr_t            gVisitedSpots[kMostVisitedSpots] = {};
    int                  gNextVisitedSpot = 0;
    uint32_t             gRandomState = 0x2545F491u;

#if defined(_DEBUG)
    const AIActionHeliPursuit* gLoggedAction = nullptr;
    int                        gLoggedMode = -1;

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
        return std::clamp((gSearchRadius - AIActionHeliPursuit::sSettings.SearchRadiusFirst) / span, 0.0f, 1.0f);
    }

    void AdvanceSearchClock(float pursuitTime) {
        float dT = pursuitTime - gLastPursuitTime;
        if (dT < 0.0f) dT = pursuitTime;
        if (dT < 0.0f || dT > kLongestSearchStep) dT = 0.0f;
        gSearchClock += dT;
        gLastPursuitTime = pursuitTime;
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
    for (const AIActionHeliPursuit* known : gConstructed)
        if (known == action) return;
    gConstructed[gNextConstructed] = action;
    Log::Info("AIActionHeliPursuit::AIActionHeliPursuit  ecx=0x%08X gConstructed[%d]", registers->ecx, gNextConstructed);
    gNextConstructed = (gNextConstructed + 1) % kTrackedActions;
}

AIActionHeliPursuit* AIActionHeliPursuit::Find(const IRigidBody* heliRigidBody) {
    if (IsAlive(gCurrent, heliRigidBody)) return gCurrent;
    AIActionHeliPursuit* const previous = gCurrent;
    gCurrent = nullptr;
    for (AIActionHeliPursuit* action : gConstructed) {
        if (IsAlive(action, heliRigidBody)) {
            gCurrent = action;
            break;
        }
    }
    if (gCurrent != previous)
        Log::Info("AIActionHeliPursuit  current 0x%p -> 0x%p  vtbl=0x%08X mIRigidBody[+0x54]=0x%p", previous, gCurrent, kVTable, heliRigidBody);
    return gCurrent;
}

void AIActionHeliPursuit::StraightLinePursuit() {
    Game::ThisCall<void>(kStraightLinePursuit, this);
}

void AIActionHeliPursuit::ChasePerp() {
    StraightLinePursuit();
    if (mPlayerRigidBody == nullptr) return;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    const float speed = std::sqrt(perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z);
    if (speed < kEscapeHeadingSpeed) return;
    gEscapeHeadingX = perpLinVel.x / speed;
    gEscapeHeadingZ = perpLinVel.z / speed;
}

void AIActionHeliPursuit::StartSearch() {
    gSearchRadius = sSettings.SearchRadiusFirst - kSearchRadiusGrowth;
    gSearchLapTurned = 0.0f;
    gSearchDirection = RandomUnit() < 0.5f ? 1.0f : -1.0f;
    gPointsSinceHover = 0;
    gHoverHere = false;
    gHoverEnd = -1.0f;
    gLastPursuitTime = mPursuitTime;
    for (uintptr_t& visited : gVisitedSpots) visited = 0;
    gNextVisitedSpot = 0;
    Log::Info("AIActionHeliPursuit 0x%p  StartSearch  mPursuitTime[+0x60]=%.2f dir=%+.0f escape=(%.3f, %.3f) mSearchPatternAngle[+0x70]=%.3f", this,
              mPursuitTime, gSearchDirection, gEscapeHeadingX, gEscapeHeadingZ, mSearchPatternAngle);
    SetNextSearchPoint();
}

void AIActionHeliPursuit::SetNextSearchPoint() {
    IPursuit* ip = mIVehicleAI->GetPursuit();
    if (ip == nullptr) return;

    const UMath::Vector3& lastKnown = ip->GetLastKnownLocation();
    const UMath::Vector3  myPosition = mIRigidBody->GetPosition();

    gSearchRadius = std::min(gSearchRadius + kSearchRadiusGrowth, sSettings.SearchRadiusLargest);
    const float progress = SearchProgress();
    gSearchSpeed  = Lerp(sSettings.SearchSpeedMin, sSettings.SearchSpeedMax, progress);
    gSearchHeight = Lerp(kSearchHeightNear, kSearchHeightFar, progress);
    gHoverHere    = ++gPointsSinceHover >= kPointsPerHover;

    UMath::Vector3 spot = {};
    if (gHoverHere && FindHidingSpot(lastKnown, gSearchRadius, myPosition, spot)) {
        mSearchDestPoint = spot;
        Log::Info("AIActionHeliPursuit 0x%p  SetNextSearchPoint  hiding spot (%.2f, %.2f, %.2f) radius=%.1f", this, spot.x, spot.y, spot.z,
                  gSearchRadius);
    } else {
        const bool  haveHeading = gEscapeHeadingX != 0.0f || gEscapeHeadingZ != 0.0f;
        const float headingTurn = haveHeading ? std::atan2(gEscapeHeadingZ, gEscapeHeadingX) / kTwoPi : 0.0f;
        float       offset = 0.0f;
        if (haveHeading) {
            gSearchDirection = -gSearchDirection;
            offset = gSearchDirection * RandomRange(0.0f, Lerp(kSearchConeFirst, kSearchConeLargest, progress));
            mSearchPatternAngle = WrapTurn(headingTurn + offset);
        } else {
            const float step = RandomRange(kSearchSmallestStep, kSearchLargestStep);
            gSearchLapTurned += step;
            if (gSearchLapTurned >= 1.0f) {
                gSearchLapTurned -= 1.0f;
                if (RandomUnit() < 0.5f) gSearchDirection = -gSearchDirection;
            }
            mSearchPatternAngle = WrapTurn(mSearchPatternAngle + step * gSearchDirection);
        }

        float groundX = 0.0f;
        float groundZ = 0.0f;
        if (GroundUnitsDirection(ip, mIVehicle, lastKnown, groundX, groundZ)) {
            const float angle = mSearchPatternAngle * kTwoPi;
            if (std::cos(angle) * groundX + std::sin(angle) * groundZ > kGroundUnitSide)
                mSearchPatternAngle = WrapTurn(haveHeading ? headingTurn - offset : mSearchPatternAngle + 0.5f);
        }

        const float radius = gSearchRadius * RandomRange(1.0f - kSearchRadiusJitter, 1.0f + kSearchRadiusJitter);
        const float angle = mSearchPatternAngle * kTwoPi;
        mSearchDestPoint = { lastKnown.x + std::cos(angle) * radius, lastKnown.y, lastKnown.z + std::sin(angle) * radius };
        mIAIHelicopter->RestrictPointToRoadNet(mSearchDestPoint);
    }
    mIAIHelicopter->FilterHeliAltitude(mSearchDestPoint);
    mSearchDestPoint.y += gSearchHeight;
    Log::Info("AIActionHeliPursuit 0x%p  SetNextSearchPoint  GetLastKnownLocation=(%.2f, %.2f, %.2f) radius=%.1f speed=%.1f height=%.1f hover=%d "
              "mSearchPatternAngle[+0x70]=%.3f mSearchDestPoint[+0x74]=(%.2f, %.2f, %.2f)",
              this, lastKnown.x, lastKnown.y, lastKnown.z, gSearchRadius, gSearchSpeed, gSearchHeight, gHoverHere, mSearchPatternAngle,
              mSearchDestPoint.x, mSearchDestPoint.y, mSearchDestPoint.z);
}

void AIActionHeliPursuit::SearchForPerp() {
    AdvanceSearchClock(mPursuitTime);
    const UMath::Vector3 myPosition = mIRigidBody->GetPosition();

    if (gHoverEnd >= 0.0f) {
        if (gSearchClock < gHoverEnd) {
            HoverAndLook(myPosition);
            return;
        }
        gHoverEnd = -1.0f;
        SetNextSearchPoint();
    } else if (UMath::Distancexz(myPosition, mSearchDestPoint) < kSearchNextPointRange) {
        if (gHoverHere) {
            gHoverHere = false;
            gPointsSinceHover = 0;
            gHoverPoint = myPosition;
            gHoverStart = gSearchClock;
            gHoverEnd = gSearchClock + RandomRange(kHoverShortest, kHoverLongest);
            gHoverLookAngle = mSearchPatternAngle;
            HoverAndLook(myPosition);
            return;
        }
        SetNextSearchPoint();
    }

    mIVehicleAI->SetDriveSpeed(gSearchSpeed);
    mIAIHelicopter->SetDestinationVelocity(mIRigidBody->GetLinearVelocity());
    mIVehicleAI->SetDriveTarget(mSearchDestPoint);
    mIAIHelicopter->SetLookAtPosition(SearchLookAt(myPosition));
    mIVehicleAI->DoDriving(kSearchDrivingFlags);
}

void AIActionHeliPursuit::HoverAndLook(const UMath::Vector3& myPosition) {
    const float duration = gHoverEnd - gHoverStart;
    const float turned = duration > 0.0f ? (gSearchClock - gHoverStart) / duration : 1.0f;
    const float angle = (gHoverLookAngle + turned) * kTwoPi;
    const UMath::Vector3 lookAt = { myPosition.x + std::cos(angle) * kLookDistance, gHoverPoint.y - kHoverLookDrop,
                                    myPosition.z + std::sin(angle) * kLookDistance };
    const UMath::Vector3 still = {};

    mIVehicleAI->SetDriveSpeed(0.0f);
    mIAIHelicopter->SetDestinationVelocity(still);
    mIVehicleAI->SetDriveTarget(gHoverPoint);
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
    if (this != gLoggedAction || mPursuitMode != gLoggedMode) {
        Log::Info("AIActionHeliPursuit 0x%p  mPursuitMode[+0xA4] %d %s -> %d %s  mPursuitTime[+0x60]=%.2f mSkidKnockTimer[+0x64]=%.2f", this,
                  gLoggedMode, PursuitModeName(gLoggedMode), mPursuitMode, PursuitModeName(mPursuitMode), mPursuitTime, mSkidKnockTimer);
        gLoggedAction = this;
        gLoggedMode = mPursuitMode;
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
              mSearchPatternAngle, mSearchDestPoint.x, mSearchDestPoint.y, mSearchDestPoint.z, gSearchRadius, gSearchSpeed, gSearchDirection,
              gSearchLapTurned, gEscapeHeadingX, gEscapeHeadingZ);

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
