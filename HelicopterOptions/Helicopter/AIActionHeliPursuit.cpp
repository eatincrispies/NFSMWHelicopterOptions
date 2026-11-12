#include <math.h>
#include "AIActionHeliPursuit.hpp"
#include "AIVehicleHelicopter.hpp"
#include "HeliSheet.hpp"

namespace {

    #define HELI_PURSUIT_VTABLE              0x00891358
    #define HELI_PURSUIT_CONSTRUCTOR         0x00420EC0
    #define STRAIGHT_LINE_PURSUIT            0x00412770
    #define SET_NEXT_PERP_SEARCH_DEST        0x00412F30
    #define SEARCH_FOR_PERP                  0x00413090
    #define SKID_HIT_PURSUIT                 0x00412B40
    #define B_RANDOM                         0x0045DA30
    #define GAME_SKID_KNOCK_TIME             8

    #define TRACKED_ACTIONS                  16
    #define MOST_IN_SQUAD                    8
    #define SQUAD_TIMEOUT_SECONDS            1
    #define TURN_GAP_SECONDS                 2
    #define LEAD_PATIENCE_SECONDS            15

    #define ESCAPE_HEADING_SPEED             5
    #define REVERSE_SPEED                    2
    #define AHEAD_REACH                      40
    #define BEHIND_REACH                     12
    #define FAR_SEEK                         300
    #define MID_SEEK                         15
    #define NEAR_SEEK                        3
    #define SHORTEST_LEAD                    1
    #define COVER_LOOK_AHEAD_MS              1600
    #define COVER_LOOK_STEP_MS               200

    #define SKID_HIT_NOT_YET_MS              (-4900)
    #define APPROACH_LEAD_MS                 230
    #define APPROACH_PATIENCE_SECONDS        2
    #define GAME_STRIKE_START                4
    #define STRIKE_START_MS_PER_SPEED        120
    #define GAME_STRIKE_SECONDS              2
    #define SIDE_CLEARANCE_MM                1900
    #define STRIKE_REACH                     25
    #define UNDER_CAR                        (-2)
    #define STRIKE_HOLD_MS                   250
    #define LONGEST_STRIKE_SECONDS           4
    #define STRIKE_LEAD_MS                   92
    #define STRIKE_CROSSING_PERCENT          50
    #define SKID_HIT_SIDE                    6
    #define LINE_UP_DISTANCE                 5
    #define STRIKE_HEIGHT_MM                 450
    #define SWEEP_HEIGHT_MM                  1500
    #define TOUCH_REACH_MM                   300
    #define BOX_AXES                         3
    #define AFTER_HIT_MS                     300
    #define BOUNCE_SIDE                      6
    #define BOUNCE_HEIGHT                    3

    #define PARKED_SPEED                     2
    #define SKID_HIT_NEAREST                 5
    #define SKID_HIT_FARTHEST                35
    #define YDELT_FOR_SKID_KNOCK             13
    #define TIME_BETWEEN_SKID_HIT_ATTEMPTS   5
    #define STRATEGY_CHOICES                 2

    #define IMPACT_SPEED_LOSS_PERCENT        30
    #define IMPACT_KNOCK                     15
    #define UPRIGHT_HOLD_MS                  1200
    #define UPRIGHT_SPIN_DEGREES             45
    #define PUSH_MS                          1200
    #define PUSH_SPEED                       8
    #define PUSH_SPEED_LOSS_PERCENT          24
    #define PUSH_LOST_DISTANCE               15
    #define PUSH_HOLD_RATE                   5

    #define LONGEST_CLOCK_STEP_SECONDS       1
    #define SEARCH_NEXT_POINT_RANGE          30
    #define SEARCH_RADIUS_GROWTH             40
    #define SEARCH_RADIUS_JITTER_PERCENT     25
    #define SEARCH_SMALLEST_STEP_PERCENT     8
    #define SEARCH_LARGEST_STEP_PERCENT      17
    #define SEARCH_CONE_FIRST_PERCENT        17
    #define SEARCH_CONE_LARGEST_PERCENT      50
    #define SEARCH_HEIGHT_NEAR               5
    #define SEARCH_HEIGHT_FAR                25
    #define SEARCH_DRIVING_FLAGS             7
    #define STARTING_SEARCH_RADIUS           80
    #define STARTING_SEARCH_SPEED            15
    #define POINTS_PER_HOVER                 3
    #define HOVER_SHORTEST_SECONDS           3
    #define HOVER_LONGEST_SECONDS            5
    #define HOVER_LOOK_DROP                  20
    #define NOT_HOVERING                     (-1)
    #define LOOK_SWEEP_MILLIRADIANS          790
    #define LOOK_SWEEP_PERIOD_SECONDS        10
    #define LOOK_DISTANCE                    60
    #define LOOKING_AT_DEST_MM               100
    #define GROUND_UNIT_RANGE                400
    #define GROUND_UNIT_SIDE_PERCENT         50
    #define MOST_GROUND_UNITS                32
    #define MOST_VISITED_SPOTS               8
    #define MOST_ZONES_SCANNED               4096
    #define HALF_TURN_PERCENT                50
    #define COIN_FLIP_PERCENT                50

    #define RANDOM_SEED                      0x2545F491
    #define RANDOM_MASK                      0xFFFFFF
    #define RANDOM_RANGE                     16777216

    #define RADIANS_PER_TURN                 6.2831853f
    #define RADIANS_PER_DEGREE               0.017453292f

    typedef int (__cdecl* RandomFunction)(int range);

    const BYTE kConstructorCode[] = { 0x6A, 0xFF, 0x68, 0xF8, 0x7D, 0x86, 0x00 };

    const Patch::FloatOperand kStraightLineLeadBase      = { 0x00412946, { 0xD8, 0x05 }, 0x00890614 };
    const Patch::FloatOperand kStraightLineLeadMaxTest   = { 0x0041294C, { 0xD8, 0x15 }, 0x00890658 };
    const Patch::FloatOperand kStraightLineLeadMaxClamp  = { 0x0041295B, { 0xD9, 0x05 }, 0x00890658 };
    const Patch::FloatOperand kStraightLineClosingNear   = { 0x00412AA0, { 0xD9, 0x05 }, 0x00890560 };
    const Patch::FloatOperand kStraightLineClosingMid    = { 0x00412A6E, { 0xD9, 0x05 }, 0x00890614 };
    const Patch::FloatOperand kStraightLineClosingFar    = { 0x00412A87, { 0xD9, 0x05 }, 0x00890DD0 };
    const Patch::FloatStore   kStraightLineSkidKnockTime = { 0x004128B2, { 0xC7, 0x46, 0x64 }, GAME_SKID_KNOCK_TIME };
    const Patch::CallSite     kUpdateStraightLinePursuit = { 0x004278C0, STRAIGHT_LINE_PURSUIT };
    const Patch::CallSite     kUpdateStartSearch         = { 0x00427881, SET_NEXT_PERP_SEARCH_DEST };
    const Patch::CallSite     kUpdateSearchForPerp       = { 0x004278E6, SEARCH_FOR_PERP };
    const Patch::CallSite     kUpdateSkidHitPursuit      = { 0x004278DB, SKID_HIT_PURSUIT };

    struct FormationSpot {
        int side;
        int back;
        int up;
    };

    const FormationSpot kFormationSpots[] = { { 22, 10, 5 }, { -22, 10, 5 }, { 0, 60, 10 } };
    const int           kSearchSpreadPercent[] = { 0, 15, -15, 50 };
    const int           kHidingZoneTypes[] = {
        TrackPathManager::TRACK_PATH_ZONE_HIDDEN,
        TrackPathManager::TRACK_PATH_ZONE_TUNNEL,
        TrackPathManager::TRACK_PATH_ZONE_OVERPASS,
        TrackPathManager::TRACK_PATH_ZONE_OVERPASS_SMALL,
    };

    #define FORMATION_SPOTS                  ((int)(sizeof(kFormationSpots) / sizeof(kFormationSpots[0])))
    #define SEARCH_SPREADS                   ((int)(sizeof(kSearchSpreadPercent) / sizeof(kSearchSpreadPercent[0])))
    #define HIDING_ZONE_TYPES                ((int)(sizeof(kHidingZoneTypes) / sizeof(kHidingZoneTypes[0])))

    C_ASSERT(offsetof(AIActionHeliPursuit, mIVehicleAI) == 0x4C);
    C_ASSERT(offsetof(AIActionHeliPursuit, mIRigidBody) == 0x54);
    C_ASSERT(offsetof(AIActionHeliPursuit, mIAIHelicopter) == 0x58);
    C_ASSERT(offsetof(AIActionHeliPursuit, mPursuitTime) == 0x60);
    C_ASSERT(offsetof(AIActionHeliPursuit, mSkidKnockTimer) == 0x64);
    C_ASSERT(offsetof(AIActionHeliPursuit, mPathTime) == 0x68);
    C_ASSERT(offsetof(AIActionHeliPursuit, mBuildingPath) == 0x6C);
    C_ASSERT(offsetof(AIActionHeliPursuit, mSearchPatternAngle) == 0x70);
    C_ASSERT(offsetof(AIActionHeliPursuit, mSearchDestPoint) == 0x74);
    C_ASSERT(offsetof(AIActionHeliPursuit, mPlayerRigidBody) == 0x80);
    C_ASSERT(offsetof(AIActionHeliPursuit, mPlayerPosition) == 0x84);
    C_ASSERT(offsetof(AIActionHeliPursuit, mSkidHitOffset) == 0x90);
    C_ASSERT(offsetof(AIActionHeliPursuit, mCollisionAbort) == 0x9C);
    C_ASSERT(offsetof(AIActionHeliPursuit, mPlayerSpeed) == 0xA0);
    C_ASSERT(offsetof(AIActionHeliPursuit, mPursuitMode) == 0xA4);

    struct LeadDistance {
        float base;
        float max;
    };

    struct ClosingSpeed {
        float nearSpeed;
        float midSpeed;
        float farSpeed;
    };

    struct PursuitState {
        PursuitState()
            : place(0),
              attacked(false),
              approaching(false),
              approachStart(0),
              striking(false),
              strikeHit(false),
              sweeping(false),
              strikeStart(0),
              strikeClock(0),
              lastStrikeTime(0),
              pushUntil(0),
              pushCap(0),
              searchRadius(STARTING_SEARCH_RADIUS),
              searchSpeed(STARTING_SEARCH_SPEED),
              searchDirection(1),
              searchLapTurned(0),
              searchHeight(SEARCH_HEIGHT_NEAR),
              searchClock(0),
              lastPursuitTime(0),
              pointsSinceHover(0),
              hoverHere(false),
              hoverStart(0),
              hoverEnd(NOT_HOVERING),
              hoverLookAngle(0) {
            hoverPoint = UMath::Vector(0, 0, 0);
            pushAway = UMath::Vector(0, 0, 0);
            pushOffset = UMath::Vector(0, 0, 0);
        }

        int            place;
        bool           attacked;
        bool           approaching;
        float          approachStart;
        bool           striking;
        bool           strikeHit;
        bool           sweeping;
        float          strikeStart;
        float          strikeClock;
        float          lastStrikeTime;
        float          pushUntil;
        float          pushCap;
        UMath::Vector3 pushAway;
        UMath::Vector3 pushOffset;
        float          searchRadius;
        float          searchSpeed;
        float          searchDirection;
        float          searchLapTurned;
        float          searchHeight;
        float          searchClock;
        float          lastPursuitTime;
        int            pointsSinceHover;
        bool           hoverHere;
        float          hoverStart;
        float          hoverEnd;
        float          hoverLookAngle;
        UMath::Vector3 hoverPoint;
    };

    class Squad {
      public:
        Squad() : mCount(0), mLeadSince(0) {}

        int Join(const AIActionHeliPursuit* action, float now) {
            Prune(now);
            int place = Place(action);
            if (place >= 0) {
                mMembers[place].seen = now;
                return place;
            }
            if (mCount == MOST_IN_SQUAD) return MOST_IN_SQUAD - 1;
            if (mCount == 0) mLeadSince = now;
            mMembers[mCount].action = action;
            mMembers[mCount].seen = now;
            return mCount++;
        }

        int Place(const AIActionHeliPursuit* action) const {
            for (int i = 0; i < mCount; ++i)
                if (mMembers[i].action == action) return i;
            return -1;
        }

        void SendToBack(const AIActionHeliPursuit* action, float now) {
            int place = Place(action);
            if (place < 0 || place == mCount - 1) return;
            Member member = mMembers[place];
            for (int i = place; i + 1 < mCount; ++i) mMembers[i] = mMembers[i + 1];
            mMembers[mCount - 1] = member;
            if (place == 0) mLeadSince = now;
        }

        bool MayAttack(const AIActionHeliPursuit* action, float now) const {
            if (mCount <= 1) return true;
            return mMembers[0].action == action && now >= mLeadSince && now - mLeadSince >= TURN_GAP_SECONDS;
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
                if (now >= mMembers[i].seen && now - mMembers[i].seen <= SQUAD_TIMEOUT_SECONDS) {
                    mMembers[kept++] = mMembers[i];
                    continue;
                }
                if (kept == 0) mLeadSince = now;
            }
            mCount = kept;
        }

        Member mMembers[MOST_IN_SQUAD];
        int    mCount;
        float  mLeadSince;
    };

    struct BodyBox {
        UMath::Vector3 axis[BOX_AXES];
        float          half[BOX_AXES];
    };

    LeadDistance                                gLeadDistance;
    ClosingSpeed                                 gClosingSpeed;
    bool                                         gSkidKnockPatched = false;
    float                                        gSkidKnockTime    = 0;
    AIActionHeliPursuit*                         gConstructed[TRACKED_ACTIONS];
    int                                          gNextConstructed  = 0;
    PerHelicopter<PursuitState, TRACKED_ACTIONS> gPursuits;
    PursuitState                                 gUnknownHelicopter;
    PursuitState*                                gHeli             = &gUnknownHelicopter;
    Squad                                        gSquad;
    float                                        gEscapeHeadingX   = 0;
    float                                        gEscapeHeadingZ   = 0;
    float                                        gLastSearchTime   = -1;
    const TrackPathZone*                         gVisitedSpots[MOST_VISITED_SPOTS];
    int                                          gNextVisitedSpot  = 0;
    DWORD                                        gRandomState      = RANDOM_SEED;
    IRigidBody*                                  gUprightCar       = NULL;
    float                                        gUprightUntil     = 0;

    int            bRandom(int range);
    bool           IsAlive(const AIActionHeliPursuit* action, const IRigidBody* heliRigidBody);
    int            JoinSquad(const AIActionHeliPursuit* action, float now);
    void           AdvanceClock(float& clock, float& lastTime, float pursuitTime);
    float          RandomUnit();
    float          RandomRange(float low, float high);
    bool           Chance(int percent);
    float          Lerp(float from, float to, float t);
    float          WrapTurn(float turn);
    float          Radians(int milliradians);
    float          SearchProgress();
    bool           WasVisited(const TrackPathZone* zone);
    void           MarkVisited(const TrackPathZone* zone);
    bool           FindHidingSpot(const UMath::Vector3& lastKnown, float radius, const UMath::Vector3& myPosition, UMath::Vector3& spot);
    bool           GroundUnitsDirection(IPursuit* ip, const IVehicle* heli, const UMath::Vector3& lastKnown, float& x, float& z);
    BodyBox        BoxOf(const IRigidBody* body);
    float          Extent(const BodyBox& box, const UMath::Vector3& axis);
    bool           BoxesTouch(const BodyBox& a, const BodyBox& b, const UMath::Vector3& apart, float reach);

}

AIActionHeliPursuit::Settings AIActionHeliPursuit::sSettings;

AIActionHeliPursuit::Settings::Settings()
    : LeadBase(30.0f),
      LeadMax(45.0f),
      AttackDuration(8.0f),
      ClosingSpeedNear(20.0f),
      ClosingSpeedMid(30.0f),
      ClosingSpeedFar(50.0f),
      SearchSpeedMin(25.0f),
      SearchSpeedMax(40.0f),
      SearchRadiusFirst(80.0f),
      SearchRadiusLargest(250.0f) {
}

void AIActionHeliPursuit::InstallPatches() {
    Refresh();

    Patch::Begin();
    Patch::RedirectFloat(kStraightLineLeadBase, &gLeadDistance.base);
    Patch::RedirectFloat(kStraightLineLeadMaxTest, &gLeadDistance.max);
    Patch::RedirectFloat(kStraightLineLeadMaxClamp, &gLeadDistance.max);
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectFloat(kStraightLineClosingNear, &gClosingSpeed.nearSpeed);
    Patch::RedirectFloat(kStraightLineClosingMid, &gClosingSpeed.midSpeed);
    Patch::RedirectFloat(kStraightLineClosingFar, &gClosingSpeed.farSpeed);
    Patch::Commit();

    Patch::Begin();
    Patch::StoreFloat(kStraightLineSkidKnockTime, sSettings.AttackDuration);
    gSkidKnockPatched = Patch::Commit();
    gSkidKnockTime = sSettings.AttackDuration;

    Patch::Begin();
    Patch::RedirectCall(kUpdateStraightLinePursuit, MethodAddress(&AIActionHeliPursuit::ChasePerp));
    Patch::RedirectCall(kUpdateStartSearch, MethodAddress(&AIActionHeliPursuit::StartSearch));
    Patch::RedirectCall(kUpdateSearchForPerp, MethodAddress(&AIActionHeliPursuit::SearchForPerp));
    Patch::Commit();

    Patch::Begin();
    Patch::RedirectCall(kUpdateSkidHitPursuit, MethodAddress(&AIActionHeliPursuit::StrikePerp));
    Patch::Commit();
}

bool AIActionHeliPursuit::HookConstructor() {
    return Detour::Install(HELI_PURSUIT_CONSTRUCTOR, kConstructorCode, sizeof(kConstructorCode), &ConstructorEntry);
}

void AIActionHeliPursuit::Refresh() {
    if (sSettings.SearchSpeedMin > sSettings.SearchSpeedMax) sSettings.SearchSpeedMin = sSettings.SearchSpeedMax;
    if (sSettings.SearchRadiusFirst > sSettings.SearchRadiusLargest) sSettings.SearchRadiusFirst = sSettings.SearchRadiusLargest;

    gLeadDistance.base = sSettings.LeadBase;
    gLeadDistance.max = sSettings.LeadMax;
    gClosingSpeed.nearSpeed = sSettings.ClosingSpeedNear;
    gClosingSpeed.midSpeed = sSettings.ClosingSpeedMid;
    gClosingSpeed.farSpeed = sSettings.ClosingSpeedFar;

    if (!gSkidKnockPatched || sSettings.AttackDuration == gSkidKnockTime) return;
    if (!Patch::RewriteStoredFloat(kStraightLineSkidKnockTime, gSkidKnockTime, sSettings.AttackDuration)) {
        gSkidKnockPatched = false;
        return;
    }
    gSkidKnockTime = sSettings.AttackDuration;
}

void __cdecl AIActionHeliPursuit::ConstructorEntry(Detour::Registers* registers) {
    AIActionHeliPursuit* action = (AIActionHeliPursuit*)registers->ecx;
    if (action == NULL) return;
    gPursuits.Forget(action);
    for (int i = 0; i < TRACKED_ACTIONS; ++i)
        if (gConstructed[i] == action) return;
    gConstructed[gNextConstructed] = action;
    gNextConstructed = (gNextConstructed + 1) % TRACKED_ACTIONS;
}

AIActionHeliPursuit* AIActionHeliPursuit::Find(const IRigidBody* heliRigidBody) {
    for (int i = 0; i < TRACKED_ACTIONS; ++i)
        if (IsAlive(gConstructed[i], heliRigidBody)) return gConstructed[i];
    return NULL;
}

void AIActionHeliPursuit::StraightLinePursuit() {
    ((VoidMethod)STRAIGHT_LINE_PURSUIT)(this);
}

void AIActionHeliPursuit::ChasePerp() {
    float now = Sim::GetTime();
    int joined = JoinSquad(this, now);
    if (gHeli->attacked) {
        gHeli->attacked = false;
        gSquad.SendToBack(this, now);
    } else if (joined == 0 && gSquad.Size() > 1 && mSkidKnockTimer < -LEAD_PATIENCE_SECONDS) {
        gSquad.SendToBack(this, now);
    }
    int place = gHeli->place = gSquad.Place(this);
    bool covered = IsPerpUnderCover();
    if ((covered || !gSquad.MayAttack(this, now)) && mSkidKnockTimer < Seconds(SKID_HIT_NOT_YET_MS)) mSkidKnockTimer = Seconds(SKID_HIT_NOT_YET_MS);
    StraightLinePursuit();
    if (mPursuitMode == kStraight_Line && ShouldAttackParkedPerp()) AttackParkedPerp();
    if (IsSkidHitting()) gHeli->attacked = true;
    if (mPlayerRigidBody == NULL) return;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    float speed = sqrtf(perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z);
    if (mPursuitMode == kStraight_Line && IsPerpReversing()) LeadReversing(perpLinVel, speed);
    if (mPursuitMode == kStraight_Line) HoldFormationSpot(place, perpLinVel);
    if (speed < ESCAPE_HEADING_SPEED) return;
    gEscapeHeadingX = perpLinVel.x / speed;
    gEscapeHeadingZ = perpLinVel.z / speed;
}

bool AIActionHeliPursuit::IsPerpUnderCover() const {
    if (mPlayerRigidBody == NULL) return false;
    UMath::Vector3 car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    for (int ahead = 0; ahead <= COVER_LOOK_AHEAD_MS; ahead += COVER_LOOK_STEP_MS)
        if (HeliSheet::UnderCover(car + carVelocity * Seconds(ahead), ahead == 0)) return true;
    return false;
}

bool AIActionHeliPursuit::IsPerpReversing() const {
    if (mPlayerRigidBody == NULL) return false;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    if (perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z < REVERSE_SPEED * REVERSE_SPEED) return false;
    UMath::Vector3 forward = UMath::Vector(0, 0, 0);
    mPlayerRigidBody->GetForwardVector(forward);
    return perpLinVel.x * forward.x + perpLinVel.z * forward.z < 0;
}

void AIActionHeliPursuit::LeadReversing(const UMath::Vector3& perpLinVel, float speed) {
    UMath::Vector3 player = mPlayerRigidBody->GetPosition();
    UMath::Vector3 seek = mIVehicleAI->GetDriveTarget();
    float lead = UMath::Distancexz(seek, player);
    seek.x = player.x + perpLinVel.x / speed * lead;
    seek.z = player.z + perpLinVel.z / speed * lead;
    ChaseToward(seek, perpLinVel);
    mIAIHelicopter->SetLookAtPosition(seek);
}

void AIActionHeliPursuit::HoldFormationSpot(int place, const UMath::Vector3& perpLinVel) {
    if (place <= 0) return;
    const FormationSpot& spot = kFormationSpots[(place > FORMATION_SPOTS ? FORMATION_SPOTS : place) - 1];
    UMath::Vector3 player = mPlayerRigidBody->GetPosition();
    UMath::Vector3 seek = mIVehicleAI->GetDriveTarget();
    float aheadX = seek.x - player.x;
    float aheadZ = seek.z - player.z;
    float lead = sqrtf(aheadX * aheadX + aheadZ * aheadZ);
    if (lead < SHORTEST_LEAD) return;
    aheadX /= lead;
    aheadZ /= lead;
    seek.x += aheadZ * spot.side - aheadX * spot.back;
    seek.z -= aheadX * spot.side + aheadZ * spot.back;
    seek.y += spot.up;
    ChaseToward(seek, perpLinVel);
}

void AIActionHeliPursuit::ChaseToward(const UMath::Vector3& seek, const UMath::Vector3& perpLinVel) {
    UMath::Vector3 me = mIRigidBody->GetPosition();
    float toSeekX = seek.x - me.x;
    float toSeekZ = seek.z - me.z;
    float distance = sqrtf(toSeekX * toSeekX + toSeekZ * toSeekZ);
    float adjust = distance > 0 ? (toSeekX * perpLinVel.x + toSeekZ * perpLinVel.z) / distance : 0;
    if (adjust < 0) adjust /= 2;
    float closing = distance > FAR_SEEK ? gClosingSpeed.farSpeed
                  : distance > MID_SEEK ? gClosingSpeed.midSpeed
                  : distance > NEAR_SEEK ? gClosingSpeed.nearSpeed
                                         : distance;

    mIVehicleAI->SetDriveSpeed(adjust + closing);
    mIVehicleAI->SetDriveTarget(seek);
}

void AIActionHeliPursuit::SkidHitPursuit() {
    ((VoidMethod)SKID_HIT_PURSUIT)(this);
}

void AIActionHeliPursuit::StrikePerp() {
    JoinSquad(this, Sim::GetTime());
    gHeli->attacked = true;
    AdvanceClock(gHeli->strikeClock, gHeli->lastStrikeTime, mPursuitTime);
    if (mPursuitMode != kSkid_Hit_Strike) gHeli->striking = false;
    bool pushing = gHeli->striking && IsPushing();
    bool keepDiving = gHeli->striking && (CanKeepDiving() || pushing);
    if (keepDiving && mSkidKnockTimer < Seconds(STRIKE_HOLD_MS)) mSkidKnockTimer = Seconds(STRIKE_HOLD_MS);
    if (pushing) mCollisionAbort = 0;
    kPursuitMode modeBefore = mPursuitMode;
    float timerBefore = mSkidKnockTimer;

    SkidHitPursuit();

    if (mPursuitMode == kStraight_Line && timerBefore > 0 && KeepsUpWhileReversing()) {
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
        mPursuitMode = kSkid_Hit_Strike;
        mSkidKnockTimer = Seconds(STRIKE_HOLD_MS);
    }
    if (mPursuitMode != kSkid_Hit_Strike || mPlayerRigidBody == NULL || mIRigidBody == NULL) {
        gHeli->striking = false;
        return;
    }
    if (!gHeli->striking) {
        gHeli->striking = true;
        gHeli->strikeHit = false;
        gHeli->sweeping = false;
        gHeli->strikeStart = gHeli->strikeClock;
    }
    Dive();
}

bool AIActionHeliPursuit::ShouldAttackParkedPerp() const {
    if (mPlayerRigidBody == NULL || mIRigidBody == NULL || mIVehicleAI == NULL) return false;
    if (!(mSkidKnockTimer < -TIME_BETWEEN_SKID_HIT_ATTEMPTS)) return false;
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    if (perpLinVel.x * perpLinVel.x + perpLinVel.z * perpLinVel.z >= PARKED_SPEED * PARKED_SPEED) return false;
    IPursuit* pursuit = mIVehicleAI->GetPursuit();
    if (pursuit == NULL || !pursuit->SkidHitEnabled()) return false;
    UMath::Vector3 perp2Me = mIRigidBody->GetPosition() - mPlayerRigidBody->GetPosition();
    float distance = sqrtf(UMath::Dot(perp2Me, perp2Me));
    return distance > SKID_HIT_NEAREST && distance < SKID_HIT_FARTHEST && perp2Me.y < YDELT_FOR_SKID_KNOCK;
}

void AIActionHeliPursuit::AttackParkedPerp() {
    mPursuitMode = kSkid_Hit_Approach;
    mSkidKnockTimer = sSettings.AttackDuration;
    mSkidHitOffset = UMath::Vector(0, 0, 0);
    EAXAirSupport* airSupport = EAXAirSupport::SoundAIHeli();
    if (airSupport != NULL)
        airSupport->SelfStrategy(bRandom(STRATEGY_CHOICES) != 0 ? EAXAirSupport::Type_self_strategy_type_Rolling_Roadblock
                                                                : EAXAirSupport::Type_self_strategy_type_Herding);
}

bool AIActionHeliPursuit::KeepsUpWhileReversing() const {
    if (mIRigidBody == NULL || !IsPerpReversing()) return false;
    UMath::Vector3 car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& perpLinVel = mPlayerRigidBody->GetLinearVelocity();
    UMath::Vector3 heli = mIRigidBody->GetPosition();
    UMath::Vector3 apart = heli - car;
    float distanceSq = UMath::Dot(apart, apart);
    if (apart.x * perpLinVel.x + apart.z * perpLinVel.z > 0) return distanceSq <= AHEAD_REACH * AHEAD_REACH && heli.y >= car.y;
    return distanceSq <= BEHIND_REACH * BEHIND_REACH;
}

bool AIActionHeliPursuit::ShouldStrikeNow() const {
    if (mPlayerRigidBody == NULL || mIRigidBody == NULL) return false;
    UMath::Vector3 car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    UMath::Vector3 heli = mIRigidBody->GetPosition();
    UMath::Vector3 right = UMath::Vector(0, 0, 0);
    mPlayerRigidBody->GetRightVector(right);
    UMath::Vector3 apart = heli - car;
    if (fabsf(UMath::Dot(apart, right)) <= Metres(SIDE_CLEARANCE_MM)) return false;

    UMath::Vector3 lineUp = car + carVelocity * Seconds(APPROACH_LEAD_MS) + mSkidHitOffset;
    float speed = sqrtf(carVelocity.x * carVelocity.x + carVelocity.z * carVelocity.z);
    if (UMath::Distancexz(heli, lineUp) <= Larger(GAME_STRIKE_START, speed * Seconds(STRIKE_START_MS_PER_SPEED))) return true;
    return gHeli->strikeClock - gHeli->approachStart >= APPROACH_PATIENCE_SECONDS && UMath::Dot(apart, apart) <= STRIKE_REACH * STRIKE_REACH;
}

void AIActionHeliPursuit::BeginStrike() {
    mPursuitMode = kSkid_Hit_Strike;
    if (mSkidKnockTimer < GAME_STRIKE_SECONDS) mSkidKnockTimer = GAME_STRIKE_SECONDS;
    gHeli->approaching = false;
    EAXAirSupport* airSupport = EAXAirSupport::SoundAIHeli();
    if (airSupport != NULL) airSupport->IntentToRam();
}

bool AIActionHeliPursuit::CanKeepDiving() const {
    if (mPursuitMode != kSkid_Hit_Strike || gHeli->strikeHit || mPlayerRigidBody == NULL || mIRigidBody == NULL) return false;
    if (gHeli->strikeClock - gHeli->strikeStart >= LONGEST_STRIKE_SECONDS) return false;
    UMath::Vector3 apart = mIRigidBody->GetPosition() - mPlayerRigidBody->GetPosition();
    return apart.y > UNDER_CAR && UMath::Dot(apart, apart) <= STRIKE_REACH * STRIKE_REACH;
}

void AIActionHeliPursuit::Dive() {
    UMath::Vector3 car = mPlayerRigidBody->GetPosition();
    const UMath::Vector3& carVelocity = mPlayerRigidBody->GetLinearVelocity();
    UMath::Vector3 heli = mIRigidBody->GetPosition();
    UMath::Vector3 apart = heli - car;

    UMath::Vector3 right = UMath::Vector(0, 0, 0);
    mPlayerRigidBody->GetRightVector(right);
    float side = UMath::Dot(apart, right);
    UMath::Vector3 outward = right * (float)(side < 0 ? -1 : 1);

    bool touching = BoxesTouch(BoxOf(mIRigidBody), BoxOf(mPlayerRigidBody), apart, Metres(TOUCH_REACH_MM));
    if (!gHeli->strikeHit && touching) {
        gHeli->strikeHit = true;
        if (mSkidKnockTimer > Seconds(AFTER_HIT_MS)) mSkidKnockTimer = Seconds(AFTER_HIT_MS);
        HitCar(outward);
    }
    float lift = HeliSheet::GroundLift(mIRigidBody);
    if (!gHeli->sweeping && (apart.y <= Metres(SWEEP_HEIGHT_MM) || lift > 0)) gHeli->sweeping = true;

    UMath::Vector3 target = car + carVelocity * Seconds(STRIKE_LEAD_MS);
    if (gHeli->strikeHit && IsPushing()) {
        PushCar();
        target.x += gHeli->pushOffset.x;
        target.z += gHeli->pushOffset.z;
        target.y = car.y + Metres(STRIKE_HEIGHT_MM);
    } else if (gHeli->strikeHit) {
        target = target + outward * BOUNCE_SIDE;
        target.y = car.y + BOUNCE_HEIGHT;
    } else {
        target = target + mSkidHitOffset * (gHeli->sweeping ? -Fraction(STRIKE_CROSSING_PERCENT) : (float)LINE_UP_DISTANCE / SKID_HIT_SIDE);
        target.y = car.y + Metres(STRIKE_HEIGHT_MM);
    }
    if (lift > 0) target.y = Larger(target.y, heli.y + lift);
    mIVehicleAI->SetDriveTarget(target);
}

bool AIActionHeliPursuit::IsPushing() const {
    if (!gHeli->strikeHit || gHeli->strikeClock >= gHeli->pushUntil || mPlayerRigidBody == NULL || mIRigidBody == NULL) return false;
    UMath::Vector3 apart = mIRigidBody->GetPosition() - mPlayerRigidBody->GetPosition();
    return UMath::Dot(apart, apart) <= PUSH_LOST_DISTANCE * PUSH_LOST_DISTANCE;
}

void AIActionHeliPursuit::HitCar(const UMath::Vector3& outward) {
    UMath::Vector3 away = outward * -1;
    UMath::Vector3 before = mPlayerRigidBody->GetLinearVelocity();
    UMath::Vector3 velocity = before + away * IMPACT_KNOCK;
    velocity.x -= before.x * Fraction(IMPACT_SPEED_LOSS_PERCENT);
    velocity.z -= before.z * Fraction(IMPACT_SPEED_LOSS_PERCENT);
    mPlayerRigidBody->SetLinearVelocity(velocity);

    float flat = sqrtf(away.x * away.x + away.z * away.z);
    gHeli->pushAway = flat > 0 ? UMath::Vector(away.x / flat, 0, away.z / flat) : UMath::Vector(0, 0, 0);
    float across = velocity.x * gHeli->pushAway.x + velocity.z * gHeli->pushAway.z;
    float alongX = velocity.x - gHeli->pushAway.x * across;
    float alongZ = velocity.z - gHeli->pushAway.z * across;
    gHeli->pushCap = sqrtf(alongX * alongX + alongZ * alongZ);
    gHeli->pushUntil = gHeli->strikeClock + Seconds(PUSH_MS);
    UMath::Vector3 offset = mIRigidBody->GetPosition() - mPlayerRigidBody->GetPosition();
    gHeli->pushOffset = UMath::Vector(offset.x, 0, offset.z);

    gUprightCar = mPlayerRigidBody;
    gUprightUntil = Sim::GetTime() + Seconds(UPRIGHT_HOLD_MS);
    KeepCarUpright();
}

void AIActionHeliPursuit::PushCar() {
    UMath::Vector3 carVelocity = mPlayerRigidBody->GetLinearVelocity();
    UMath::Vector3 heliVelocity = mIRigidBody->GetLinearVelocity();
    if (!UMath::IsFinite(carVelocity) || !UMath::IsFinite(heliVelocity)) return;

    const UMath::Vector3& away = gHeli->pushAway;
    float across = carVelocity.x * away.x + carVelocity.z * away.z;
    float alongX = carVelocity.x - away.x * across;
    float alongZ = carVelocity.z - away.z * across;
    float along = sqrtf(alongX * alongX + alongZ * alongZ);
    float progress = Clamp(1 - (gHeli->pushUntil - gHeli->strikeClock) / Seconds(PUSH_MS), 0, 1);
    float cap = gHeli->pushCap * (1 - Fraction(PUSH_SPEED_LOSS_PERCENT) * progress);
    if (along > cap && along > 0) {
        alongX *= cap / along;
        alongZ *= cap / along;
    }
    across = Larger(across, (float)PUSH_SPEED);
    carVelocity.x = alongX + away.x * across;
    carVelocity.z = alongZ + away.z * across;
    mPlayerRigidBody->SetLinearVelocity(carVelocity);

    UMath::Vector3 hold = mPlayerRigidBody->GetPosition() + gHeli->pushOffset - mIRigidBody->GetPosition();
    heliVelocity.x = carVelocity.x + hold.x * PUSH_HOLD_RATE;
    heliVelocity.z = carVelocity.z + hold.z * PUSH_HOLD_RATE;
    mIRigidBody->SetLinearVelocity(heliVelocity);

    gUprightCar = mPlayerRigidBody;
    gUprightUntil = Sim::GetTime() + Seconds(UPRIGHT_HOLD_MS);
}

void AIActionHeliPursuit::KeepCarUpright() {
    if (gUprightCar == NULL) return;
    float now = Sim::GetTime();
    if (now >= gUprightUntil || now < gUprightUntil - Seconds(UPRIGHT_HOLD_MS) || gUprightCar != GetLocalPlayerRigidBody()) {
        gUprightCar = NULL;
        return;
    }

    UMath::Vector3 forward = UMath::Vector(0, 0, 0);
    UMath::Vector3 right = UMath::Vector(0, 0, 0);
    gUprightCar->GetForwardVector(forward);
    gUprightCar->GetRightVector(right);
    UMath::Vector3 spin = gUprightCar->GetAngularVelocity();
    float roll = UMath::Dot(spin, forward);
    float pitch = UMath::Dot(spin, right);
    float limit = UPRIGHT_SPIN_DEGREES * RADIANS_PER_DEGREE;
    float keptRoll = Clamp(roll, -limit, limit);
    float keptPitch = Clamp(pitch, -limit, limit);
    if (keptRoll == roll && keptPitch == pitch) return;
    gUprightCar->SetAngularVelocity(spin + forward * (keptRoll - roll) + right * (keptPitch - pitch));
}

void AIActionHeliPursuit::StartSearch() {
    float now = Sim::GetTime();
    JoinSquad(this, now);
    if (now < gLastSearchTime || now - gLastSearchTime > SQUAD_TIMEOUT_SECONDS) {
        for (int i = 0; i < MOST_VISITED_SPOTS; ++i) gVisitedSpots[i] = NULL;
        gNextVisitedSpot = 0;
    }
    gLastSearchTime = now;
    gHeli->searchRadius = sSettings.SearchRadiusFirst - SEARCH_RADIUS_GROWTH;
    gHeli->searchLapTurned = 0;
    gHeli->searchDirection = (float)(Chance(COIN_FLIP_PERCENT) ? 1 : -1);
    gHeli->pointsSinceHover = 0;
    gHeli->hoverHere = false;
    gHeli->hoverEnd = NOT_HOVERING;
    gHeli->lastPursuitTime = mPursuitTime;
    SetNextSearchPoint();
}

void AIActionHeliPursuit::SetNextSearchPoint() {
    IPursuit* ip = mIVehicleAI->GetPursuit();
    if (ip == NULL) return;

    const UMath::Vector3& lastKnown = ip->GetLastKnownLocation();
    UMath::Vector3 myPosition = mIRigidBody->GetPosition();

    gHeli->searchRadius = Smaller(gHeli->searchRadius + SEARCH_RADIUS_GROWTH, sSettings.SearchRadiusLargest);
    float progress = SearchProgress();
    gHeli->searchSpeed = Lerp(sSettings.SearchSpeedMin, sSettings.SearchSpeedMax, progress);
    gHeli->searchHeight = Lerp(SEARCH_HEIGHT_NEAR, SEARCH_HEIGHT_FAR, progress);
    gHeli->hoverHere = ++gHeli->pointsSinceHover >= POINTS_PER_HOVER;

    UMath::Vector3 spot = UMath::Vector(0, 0, 0);
    if (gHeli->hoverHere && FindHidingSpot(lastKnown, gHeli->searchRadius, myPosition, spot)) {
        mSearchDestPoint = spot;
    } else {
        bool haveHeading = gEscapeHeadingX != 0 || gEscapeHeadingZ != 0;
        float headingTurn = haveHeading ? atan2f(gEscapeHeadingZ, gEscapeHeadingX) / RADIANS_PER_TURN : 0;
        float offset = 0;
        if (haveHeading) {
            gHeli->searchDirection = -gHeli->searchDirection;
            offset = gHeli->searchDirection * RandomRange(0, Lerp(Fraction(SEARCH_CONE_FIRST_PERCENT), Fraction(SEARCH_CONE_LARGEST_PERCENT), progress));
            mSearchPatternAngle = WrapTurn(headingTurn + offset);
        } else {
            float step = RandomRange(Fraction(SEARCH_SMALLEST_STEP_PERCENT), Fraction(SEARCH_LARGEST_STEP_PERCENT));
            gHeli->searchLapTurned += step;
            if (gHeli->searchLapTurned >= 1) {
                gHeli->searchLapTurned -= 1;
                if (Chance(COIN_FLIP_PERCENT)) gHeli->searchDirection = -gHeli->searchDirection;
            }
            mSearchPatternAngle = WrapTurn(mSearchPatternAngle + step * gHeli->searchDirection);
        }

        float groundX = 0;
        float groundZ = 0;
        if (GroundUnitsDirection(ip, mIVehicle, lastKnown, groundX, groundZ)) {
            float angle = mSearchPatternAngle * RADIANS_PER_TURN;
            if (cosf(angle) * groundX + sinf(angle) * groundZ > Fraction(GROUND_UNIT_SIDE_PERCENT))
                mSearchPatternAngle = WrapTurn(haveHeading ? headingTurn - offset : mSearchPatternAngle + Fraction(HALF_TURN_PERCENT));
        }

        float radius = gHeli->searchRadius * RandomRange(1 - Fraction(SEARCH_RADIUS_JITTER_PERCENT), 1 + Fraction(SEARCH_RADIUS_JITTER_PERCENT));
        float angle = (mSearchPatternAngle + Fraction(kSearchSpreadPercent[ClampInt(gHeli->place, 0, SEARCH_SPREADS - 1)])) * RADIANS_PER_TURN;
        mSearchDestPoint = UMath::Vector(lastKnown.x + cosf(angle) * radius, lastKnown.y, lastKnown.z + sinf(angle) * radius);
        mIAIHelicopter->RestrictPointToRoadNet(mSearchDestPoint);
    }
    mIAIHelicopter->FilterHeliAltitude(mSearchDestPoint);
    mSearchDestPoint.y += gHeli->searchHeight;
}

void AIActionHeliPursuit::SearchForPerp() {
    gLastSearchTime = Sim::GetTime();
    JoinSquad(this, gLastSearchTime);
    AdvanceClock(gHeli->searchClock, gHeli->lastPursuitTime, mPursuitTime);
    UMath::Vector3 myPosition = mIRigidBody->GetPosition();

    if (gHeli->hoverEnd >= 0) {
        if (gHeli->searchClock < gHeli->hoverEnd) {
            HoverAndLook(myPosition);
            return;
        }
        gHeli->hoverEnd = NOT_HOVERING;
        SetNextSearchPoint();
    } else if (UMath::Distancexz(myPosition, mSearchDestPoint) < SEARCH_NEXT_POINT_RANGE) {
        if (gHeli->hoverHere) {
            gHeli->hoverHere = false;
            gHeli->pointsSinceHover = 0;
            gHeli->hoverPoint = myPosition;
            gHeli->hoverStart = gHeli->searchClock;
            gHeli->hoverEnd = gHeli->searchClock + RandomRange(HOVER_SHORTEST_SECONDS, HOVER_LONGEST_SECONDS);
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
    mIVehicleAI->DoDriving(SEARCH_DRIVING_FLAGS);
}

void AIActionHeliPursuit::HoverAndLook(const UMath::Vector3& myPosition) {
    float duration = gHeli->hoverEnd - gHeli->hoverStart;
    float turned = duration > 0 ? (gHeli->searchClock - gHeli->hoverStart) / duration : 1;
    float angle = (gHeli->hoverLookAngle + turned) * RADIANS_PER_TURN;
    UMath::Vector3 lookAt = UMath::Vector(myPosition.x + cosf(angle) * LOOK_DISTANCE, gHeli->hoverPoint.y - HOVER_LOOK_DROP,
                                          myPosition.z + sinf(angle) * LOOK_DISTANCE);
    UMath::Vector3 still = UMath::Vector(0, 0, 0);

    mIVehicleAI->SetDriveSpeed(0);
    mIAIHelicopter->SetDestinationVelocity(still);
    mIVehicleAI->SetDriveTarget(gHeli->hoverPoint);
    mIAIHelicopter->SetLookAtPosition(lookAt);
    mIVehicleAI->DoDriving(SEARCH_DRIVING_FLAGS);
}

UMath::Vector3 AIActionHeliPursuit::SearchLookAt(const UMath::Vector3& myPosition) const {
    float distance = UMath::Distancexz(myPosition, mSearchDestPoint);
    if (distance < Metres(LOOKING_AT_DEST_MM)) return mSearchDestPoint;

    float forwardX = (mSearchDestPoint.x - myPosition.x) / distance;
    float forwardZ = (mSearchDestPoint.z - myPosition.z) / distance;
    float sweep = sinf(mPursuitTime * RADIANS_PER_TURN / LOOK_SWEEP_PERIOD_SECONDS) * Radians(LOOK_SWEEP_MILLIRADIANS);
    float c = cosf(sweep);
    float s = sinf(sweep);
    return UMath::Vector(myPosition.x + (forwardX * c - forwardZ * s) * LOOK_DISTANCE, mSearchDestPoint.y,
                         myPosition.z + (forwardX * s + forwardZ * c) * LOOK_DISTANCE);
}

namespace {

    int bRandom(int range) {
        return ((RandomFunction)B_RANDOM)(range);
    }

    bool IsAlive(const AIActionHeliPursuit* action, const IRigidBody* heliRigidBody) {
        DWORD vtable = 0;
        const IRigidBody* rigidBody = NULL;
        DWORD address = (DWORD)action;
        return action != NULL && Memory::Read(address, &vtable, sizeof(vtable)) && vtable == HELI_PURSUIT_VTABLE
            && Memory::Read(address + offsetof(AIActionHeliPursuit, mIRigidBody), &rigidBody, sizeof(rigidBody)) && rigidBody == heliRigidBody;
    }

    int JoinSquad(const AIActionHeliPursuit* action, float now) {
        gHeli = &gPursuits.Use(action);
        gHeli->place = gSquad.Join(action, now);
        return gHeli->place;
    }

    void AdvanceClock(float& clock, float& lastTime, float pursuitTime) {
        float dT = pursuitTime - lastTime;
        if (dT < 0) dT = pursuitTime;
        if (dT < 0 || dT > LONGEST_CLOCK_STEP_SECONDS) dT = 0;
        clock += dT;
        lastTime = pursuitTime;
    }

    float RandomUnit() {
        gRandomState ^= gRandomState << 13;
        gRandomState ^= gRandomState >> 17;
        gRandomState ^= gRandomState << 5;
        return (float)(gRandomState & RANDOM_MASK) / RANDOM_RANGE;
    }

    float RandomRange(float low, float high) {
        return low + (high - low) * RandomUnit();
    }

    bool Chance(int percent) {
        return RandomUnit() < Fraction(percent);
    }

    float Lerp(float from, float to, float t) {
        return from + (to - from) * t;
    }

    BodyBox BoxOf(const IRigidBody* body) {
        BodyBox box;
        UMath::Vector3 size = UMath::Vector(0, 0, 0);
        body->GetRightVector(box.axis[0]);
        body->GetUpVector(box.axis[1]);
        body->GetForwardVector(box.axis[2]);
        body->GetDimension(size);
        box.half[0] = size.x;
        box.half[1] = size.y;
        box.half[2] = size.z;
        return box;
    }

    float Extent(const BodyBox& box, const UMath::Vector3& axis) {
        float extent = 0;
        for (int i = 0; i < BOX_AXES; ++i) extent += fabsf(UMath::Dot(box.axis[i], axis)) * box.half[i];
        return extent;
    }

    bool BoxesTouch(const BodyBox& a, const BodyBox& b, const UMath::Vector3& apart, float reach) {
        for (int i = 0; i < BOX_AXES; ++i) {
            if (fabsf(UMath::Dot(apart, a.axis[i])) > a.half[i] + Extent(b, a.axis[i]) + reach) return false;
            if (fabsf(UMath::Dot(apart, b.axis[i])) > b.half[i] + Extent(a, b.axis[i]) + reach) return false;
        }
        return true;
    }

    float WrapTurn(float turn) {
        turn -= floorf(turn);
        return turn;
    }

    float Radians(int milliradians) {
        return (float)milliradians / 1000;
    }

    float SearchProgress() {
        float span = AIActionHeliPursuit::sSettings.SearchRadiusLargest - AIActionHeliPursuit::sSettings.SearchRadiusFirst;
        if (span <= 0) return 1;
        return Clamp((gHeli->searchRadius - AIActionHeliPursuit::sSettings.SearchRadiusFirst) / span, 0, 1);
    }

    bool WasVisited(const TrackPathZone* zone) {
        for (int i = 0; i < MOST_VISITED_SPOTS; ++i)
            if (gVisitedSpots[i] == zone) return true;
        return false;
    }

    void MarkVisited(const TrackPathZone* zone) {
        gVisitedSpots[gNextVisitedSpot] = zone;
        gNextVisitedSpot = (gNextVisitedSpot + 1) % MOST_VISITED_SPOTS;
    }

    bool FindHidingSpot(const UMath::Vector3& lastKnown, float radius, const UMath::Vector3& myPosition, UMath::Vector3& spot) {
        const TrackPathManager& manager = TrackPathManager::Get();
        const TrackPathZone* best = NULL;
        float bestDistance = 0;
        for (int t = 0; t < HIDING_ZONE_TYPES; ++t) {
            int type = kHidingZoneTypes[t];
            const TrackPathManager::ZoneInfo& info = manager.ZoneInfoTable[type];
            if (info.NumZones <= 0) continue;
            int scanned = 0;
            for (const TrackPathZone* zone = info.pFirstZone; zone != NULL && zone < info.pLastZone && scanned < MOST_ZONES_SCANNED;
                 zone = zone->GetMemoryImageNext(), ++scanned) {
                if (zone->MemoryImageSize <= 0) break;
                if (zone->Type != type) continue;
                UMath::Vector3 centre = UMath::Vector(-(zone->BBoxMin.y + zone->BBoxMax.y) / 2, zone->Elevation != 0 ? zone->Elevation : lastKnown.y,
                                                      (zone->BBoxMin.x + zone->BBoxMax.x) / 2);
                float distance = UMath::Distancexz(centre, myPosition);
                if (UMath::Distancexz(centre, lastKnown) <= radius && !WasVisited(zone) && (best == NULL || distance < bestDistance)) {
                    best = zone;
                    bestDistance = distance;
                    spot = centre;
                }
            }
        }
        if (best == NULL) return false;
        MarkVisited(best);
        return true;
    }

    bool GroundUnitsDirection(IPursuit* ip, const IVehicle* heli, const UMath::Vector3& lastKnown, float& x, float& z) {
        IVehicle* vehicles[MOST_GROUND_UNITS] = { NULL };
        int count = IVehicle::ReadList((DWORD)ip + IPURSUIT_IVEHICLE_LIST, vehicles, MOST_GROUND_UNITS);
        float sumX = 0;
        float sumZ = 0;
        int used = 0;
        for (int i = 0; i < count; ++i) {
            const IVehicle* vehicle = vehicles[i];
            if (vehicle == NULL || vehicle == heli) continue;
            const UMath::Vector3* position = vehicle->GetPosition();
            if (position == NULL || UMath::Distancexz(*position, lastKnown) > GROUND_UNIT_RANGE) continue;
            sumX += position->x - lastKnown.x;
            sumZ += position->z - lastKnown.z;
            ++used;
        }
        float length = sqrtf(sumX * sumX + sumZ * sumZ);
        if (used == 0 || length < 1) return false;
        x = sumX / length;
        z = sumZ / length;
        return true;
    }

}
