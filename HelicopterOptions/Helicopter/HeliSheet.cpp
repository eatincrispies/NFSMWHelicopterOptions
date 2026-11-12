#include <float.h>
#include <math.h>
#include <vector>
#include "HeliSheet.hpp"
#include "AIActionHeliPursuit.hpp"
#include "AIVehicleHelicopter.hpp"
#include "SimpleChopper.hpp"

namespace {

    #define B_IGNORE_HELI_SHEET               0x0090D621
    #define NEVER_IGNORE_HELI_SHEET           0x008EB1F4
    #define HELI_SHEET_GET_ELEVATION          0x00754290
    #define FILTER_HELI_ALTITUDE_CALL         0x004099A6
    #define CHECK_HELI_SHEET_SMOOTHING_CALL   0x00417514
    #define CHECK_HELI_SHEET_WORKING_POS_CALL 0x004175FC
    #define CHECK_HELI_SHEET_PUSH             0x0041776F
    #define PUSHED_VELOCITY_ARGUMENT          0x04

    #define RETURN_PERCENT                    80
    #define ROAD_CLEARANCE                    1
    #define GROUND_CLEARANCE                  5
    #define FACE_TOLERANCE_MM                 25
    #define ABOVE_SHEET                       1
    #define ABOVE_COVER                       1
    #define ABOVE_BRIDGE                      5
    #define INSIDE_TOLERANCE                  1
    #define ROOF_ABOVE_CAR                    2
    #define ROAD_PIECE_LENGTH                 4
    #define MOST_ROAD_PIECES                  48
    #define ROAD_CELL                         32
    #define NARROWEST_ROAD                    4
    #define MOST_ROAD_SEGMENTS                0xFFFF
    #define MOST_PROFILE_ZONES                15
    #define MOST_ZONE_POINTS                  64
    #define LEAD_POSITION_MS                  450
    #define STARTING_ELEVATION                1000
    #define CLEARANCE_CELL                    2
    #define MOST_CLEARANCE_CELLS              65536
    #define CLEARANCE_TABLE_BITS              17
    #define CLEARANCE_TABLE_SIZE              (1 << CLEARANCE_TABLE_BITS)
    #define CLEARANCE_HASH                    2654435761u
    #define CELL_KEY_MASK                     0xFFFF
    #define NEW_CELLS_PER_UPDATE              40
    #define BODY_CLEARANCE                    6
    #define CABIN_CLEARANCE                   3
    #define BODY_CLEARANCE_POINTS             8
    #define CLIMB_PERCENT                     40
    #define FULL_HEIGHT_DISTANCE              6
    #define LOOK_AHEAD_MS                     1500
    #define LONGEST_LOOK_AHEAD                60
    #define LONGEST_DEST_LOOK                 80
    #define PAST_DEST                         6
    #define LOOK_STEP                         2
    #define SLOWEST_LOOK_AHEAD                1
    #define FLAT_FACE_LIMIT                   10000
    #define HALF_CELL_PERCENT                 50
    #define DEFAULT_SURFACE                   0
    #define LOW_POINTS_CHECK_HEIGHT           10
    #define GROUND_MARGIN_MM                  400
    #define PROBE_ABOVE_MM                    1000

    #define RADIANS_PER_TURN                  6.28318530718f

    typedef float (__thiscall* GetElevationMethod)(void* coord, const bVector2* point, bVector3* normalOut, bool* pointValid);

    const BYTE kCheckHeliSheetPushCode[] = { 0x8B, 0x06, 0xFF, 0x50, 0x60 };

    const Patch::CallSite kFilterHeliAltitude       = { FILTER_HELI_ALTITUDE_CALL, HELI_SHEET_GET_ELEVATION };
    const Patch::CallSite kCheckHeliSheetSmoothing  = { CHECK_HELI_SHEET_SMOOTHING_CALL, HELI_SHEET_GET_ELEVATION };
    const Patch::CallSite kCheckHeliSheetWorkingPos = { CHECK_HELI_SHEET_WORKING_POS_CALL, HELI_SHEET_GET_ELEVATION };

    const int kCoverZones[] = {
        TrackPathManager::TRACK_PATH_ZONE_TUNNEL,
        TrackPathManager::TRACK_PATH_ZONE_OVERPASS,
        TrackPathManager::TRACK_PATH_ZONE_OVERPASS_SMALL,
        TrackPathManager::TRACK_PATH_ZONE_GARAGE,
        TrackPathManager::TRACK_PATH_ZONE_DYNAMIC,
    };

    const int kBridgeZones[] = {
        TrackPathManager::TRACK_PATH_ZONE_OVERPASS,
        TrackPathManager::TRACK_PATH_ZONE_OVERPASS_SMALL,
    };

    const DWORD kRoadSurfaces[] = {
        0x19DB2F1E,
        0x35187254,
        0x6CA26F9B,
        0x3830BABB,
        0x02FC0FC9,
        0xBA404853,
        0x52E42012,
    };

    const short kLowPointsMm[][3] = {
        { 1000, -888, 3425 },
        { -1000, -888, 3425 },
        { 1000, -888, -1075 },
        { -1000, -888, -1075 },
        { 0, -243, -6275 },
    };

    #define COVER_ZONE_COUNT                  ((int)(sizeof(kCoverZones) / sizeof(kCoverZones[0])))
    #define BRIDGE_ZONE_COUNT                 ((int)(sizeof(kBridgeZones) / sizeof(kBridgeZones[0])))
    #define ROAD_SURFACE_COUNT                ((int)(sizeof(kRoadSurfaces) / sizeof(kRoadSurfaces[0])))
    #define LOW_POINT_COUNT                   ((int)(sizeof(kLowPointsMm) / sizeof(kLowPointsMm[0])))

    C_ASSERT(sizeof(bVector3) == 0x10);
    C_ASSERT(sizeof(WRoadLane) == 0x4);
    C_ASSERT(sizeof(WRoadProfile) == 0x40);
    C_ASSERT(offsetof(WRoadSegment, vEndHandle) == 0x10);
    C_ASSERT(offsetof(WRoadSegment, vStartHandle) == 0x13);
    C_ASSERT(sizeof(WRoadSegment) == 0x16);
    C_ASSERT(offsetof(WRoadNode, fProfileIndex) == 0xE);
    C_ASSERT(offsetof(WRoadNode, fSegmentIndex) == 0x12);
    C_ASSERT(sizeof(WRoadNode) == 0x20);
    C_ASSERT(sizeof(HeliSheetCoordinate) == 0x40);
    C_ASSERT(offsetof(WCollisionTri, fPt1) == 0x10);
    C_ASSERT(offsetof(WCollisionTri, fPt2) == 0x20);
    C_ASSERT(sizeof(WCollisionTri) == 0x30);
    C_ASSERT(offsetof(WWorldPos, fFaceBits) == 0x30);
    C_ASSERT(offsetof(WWorldPos, fYOffset) == 0x34);
    C_ASSERT(offsetof(WWorldPos, fSurface) == 0x38);
    C_ASSERT(sizeof(WWorldPos) == 0x3C);
    C_ASSERT(offsetof(TrackPathZone, Elevation) == 0x14);
    C_ASSERT(offsetof(TrackPathZone, BBoxMin) == 0x20);
    C_ASSERT(offsetof(TrackPathZone, NumPoints) == 0x40);
    C_ASSERT(offsetof(TrackPathZone, Points) == 0x44);
    C_ASSERT(sizeof(TrackPathZone) == 0x244);
    C_ASSERT(sizeof(TrackPathManager::ZoneInfo) == 0x4C);
    C_ASSERT(offsetof(TrackPathManager, ZoneInfoTable) == 0xC);

    struct RoadPiece {
        float x0, y0, z0, half0;
        float x1, y1, z1, half1;
        bool  trafficAllowed;
    };

    class RoadIndex {
      public:
        RoadIndex() : mSegments(NULL), mNumSegments(0), mMinX(0), mMinZ(0), mCellsX(0), mCellsZ(0) {}

        bool Under(float x, float z, float& roadY) {
            bool trafficFree = false;
            return Under(x, z, roadY, trafficFree);
        }

        bool Under(float x, float z, float& roadY, bool& trafficFree) {
            trafficFree = false;
            if (!Ready()) return false;
            int cx = (int)floorf((x - mMinX) / ROAD_CELL);
            int cz = (int)floorf((z - mMinZ) / ROAD_CELL);
            if (cx < 0 || cz < 0 || cx >= mCellsX || cz >= mCellsZ) return false;

            int cell = cz * mCellsX + cx;
            bool found = false;
            bool traffic = false;
            for (DWORD i = mCellStart[cell]; i < mCellStart[cell + 1]; ++i) {
                const RoadPiece& piece = mPieces[mCellPieces[i]];
                float dx = piece.x1 - piece.x0;
                float dz = piece.z1 - piece.z0;
                float lengthSq = dx * dx + dz * dz;
                float t = lengthSq > 0 ? ((x - piece.x0) * dx + (z - piece.z0) * dz) / lengthSq : 0;
                t = Smaller(1, Larger(0, t));
                float offX = piece.x0 + dx * t - x;
                float offZ = piece.z0 + dz * t - z;
                float half = piece.half0 + (piece.half1 - piece.half0) * t;
                if (offX * offX + offZ * offZ > half * half) continue;

                float y = piece.y0 + (piece.y1 - piece.y0) * t;
                if (!found || y > roadY) roadY = y;
                found = true;
                traffic = traffic || piece.trafficAllowed;
            }
            trafficFree = found && !traffic;
            return found;
        }

      private:
        bool Ready() {
            if (!WRoadNetwork::IsValid()) return false;
            if (WRoadNetwork::Segments() != mSegments || WRoadNetwork::NumSegments() != mNumSegments) Build();
            return !mCellStart.empty();
        }

        static float HalfWidth(const WRoadNode& node) {
            const WRoadProfile* profiles = WRoadNetwork::Profiles();
            if (profiles == NULL || node.fProfileIndex < 0 || (unsigned)node.fProfileIndex >= WRoadNetwork::NumProfiles()) return NARROWEST_ROAD;

            const WRoadProfile& profile = profiles[node.fProfileIndex];
            int zones = profile.fNumZones > MOST_PROFILE_ZONES ? MOST_PROFILE_ZONES : profile.fNumZones;
            float half = 0;
            for (int i = 0; i < zones; ++i) half = Larger(half, fabsf(profile.mLanes[i].GetOffset()) + fabsf(profile.mLanes[i].GetWidth()) / 2);
            return half > 0 ? half : NARROWEST_ROAD;
        }

        void Build() {
            mSegments = WRoadNetwork::Segments();
            mNumSegments = WRoadNetwork::NumSegments();
            mPieces.clear();
            mCellStart.clear();
            mCellPieces.clear();

            const WRoadNode* nodes = WRoadNetwork::Nodes();
            unsigned numNodes = WRoadNetwork::NumNodes();
            if (mNumSegments == 0 || mNumSegments > MOST_ROAD_SEGMENTS) return;

            for (unsigned s = 0; s < mNumSegments; ++s) {
                const WRoadSegment& segment = mSegments[s];
                if (segment.fNodeIndex[0] >= numNodes || segment.fNodeIndex[1] >= numNodes) continue;
                const WRoadNode& start = nodes[segment.fNodeIndex[0]];
                const WRoadNode& end = nodes[segment.fNodeIndex[1]];
                float startHalf = HalfWidth(start);
                float endHalf = HalfWidth(end);

                UMath::Vector3 p0 = start.fPosition;
                UMath::Vector3 p3 = end.fPosition;
                UMath::Vector3 p1 = p0;
                UMath::Vector3 p2 = p3;
                int steps = 1;
                if (segment.IsCurved()) {
                    p1 = p0 + segment.GetStartControl();
                    p2 = p3 + segment.GetEndControl();
                    float chord = UMath::Distancexz(p0, p3);
                    steps = ClampInt((int)ceilf(chord / ROAD_PIECE_LENGTH), 1, MOST_ROAD_PIECES);
                }

                UMath::Vector3 previous = p0;
                float previousHalf = startHalf;
                for (int k = 1; k <= steps; ++k) {
                    float t = (float)k / (float)steps;
                    float u = 1 - t;
                    UMath::Vector3 point = p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t);
                    float half = startHalf + (endHalf - startHalf) * t;
                    RoadPiece piece;
                    piece.x0 = previous.x;
                    piece.y0 = previous.y;
                    piece.z0 = previous.z;
                    piece.half0 = previousHalf;
                    piece.x1 = point.x;
                    piece.y1 = point.y;
                    piece.z1 = point.z;
                    piece.half1 = half;
                    piece.trafficAllowed = segment.IsTrafficAllowed();
                    mPieces.push_back(piece);
                    previous = point;
                    previousHalf = half;
                }
            }
            if (mPieces.empty()) return;

            float minX = mPieces[0].x0;
            float maxX = minX;
            float minZ = mPieces[0].z0;
            float maxZ = minZ;
            for (size_t i = 0; i < mPieces.size(); ++i) {
                const RoadPiece& piece = mPieces[i];
                float half = Larger(piece.half0, piece.half1);
                minX = Smaller(minX, Smaller(piece.x0, piece.x1) - half);
                maxX = Larger(maxX, Larger(piece.x0, piece.x1) + half);
                minZ = Smaller(minZ, Smaller(piece.z0, piece.z1) - half);
                maxZ = Larger(maxZ, Larger(piece.z0, piece.z1) + half);
            }
            mMinX = minX;
            mMinZ = minZ;
            mCellsX = (int)((maxX - minX) / ROAD_CELL) + 1;
            mCellsZ = (int)((maxZ - minZ) / ROAD_CELL) + 1;

            std::vector<DWORD> counts((size_t)mCellsX * mCellsZ + 1, 0);
            for (int pass = 0; pass < 2; ++pass) {
                for (DWORD index = 0; index < mPieces.size(); ++index) {
                    const RoadPiece& piece = mPieces[index];
                    float half = Larger(piece.half0, piece.half1);
                    int x0 = (int)((Smaller(piece.x0, piece.x1) - half - mMinX) / ROAD_CELL);
                    int x1 = (int)((Larger(piece.x0, piece.x1) + half - mMinX) / ROAD_CELL);
                    int z0 = (int)((Smaller(piece.z0, piece.z1) - half - mMinZ) / ROAD_CELL);
                    int z1 = (int)((Larger(piece.z0, piece.z1) + half - mMinZ) / ROAD_CELL);
                    int firstZ = z0 > 0 ? z0 : 0;
                    int lastZ = z1 < mCellsZ - 1 ? z1 : mCellsZ - 1;
                    int firstX = x0 > 0 ? x0 : 0;
                    int lastX = x1 < mCellsX - 1 ? x1 : mCellsX - 1;
                    for (int cz = firstZ; cz <= lastZ; ++cz)
                        for (int cx = firstX; cx <= lastX; ++cx) {
                            size_t cell = (size_t)cz * mCellsX + cx;
                            if (pass == 0)
                                ++counts[cell + 1];
                            else
                                mCellPieces[counts[cell]++] = index;
                        }
                }
                if (pass == 0) {
                    for (size_t cell = 1; cell < counts.size(); ++cell) counts[cell] += counts[cell - 1];
                    mCellStart = counts;
                    mCellPieces.resize(counts.back());
                }
            }
        }

        std::vector<RoadPiece> mPieces;
        std::vector<DWORD>     mCellStart;
        std::vector<DWORD>     mCellPieces;
        const WRoadSegment*    mSegments;
        unsigned               mNumSegments;
        float                  mMinX;
        float                  mMinZ;
        int                    mCellsX;
        int                    mCellsZ;
    };

    class RoadSurfaces {
      public:
        RoadSurfaces() : mReady(false) {
            for (int i = 0; i < ROAD_SURFACE_COUNT; ++i) mSurfaces[i] = NULL;
        }

        bool Contains(const void* surface) {
            if (surface == NULL || !Ready()) return false;
            for (int i = 0; i < ROAD_SURFACE_COUNT; ++i)
                if (mSurfaces[i] == surface) return true;
            return false;
        }

      private:
        bool Ready() {
            if (mReady) return true;
            const void* fallback = SimSurface::Lookup(DEFAULT_SURFACE);
            int found = 0;
            for (int i = 0; i < ROAD_SURFACE_COUNT; ++i) {
                const void* surface = SimSurface::Lookup(kRoadSurfaces[i]);
                mSurfaces[i] = surface != fallback ? surface : NULL;
                if (mSurfaces[i] != NULL) ++found;
            }
            mReady = found > 0;
            return mReady;
        }

        const void* mSurfaces[ROAD_SURFACE_COUNT];
        bool        mReady;
    };

    struct Course {
        Course() : heli(NULL), skidHitting(false) {
            position = velocity = dest = UMath::Vector(0, 0, 0);
        }

        const AIVehicleHelicopter* heli;
        UMath::Vector3             position;
        UMath::Vector3             velocity;
        UMath::Vector3             dest;
        bool                       skidHitting;
    };

    class ClearanceMap {
      public:
        ClearanceMap() : mCount(0), mNewCells(0) {}

        void NewUpdate() {
            mNewCells = 0;
        }

        bool Height(float x, float z, float& height);
        bool Height(float x, float z, float& height, bool& solid);
        bool Top(float x, float z, float& top);

      private:
        struct Cell {
            DWORD key;
            float height;
            float top;
            float sheet;
            float bridgeElevation;
            bool  bridge;
            bool  known;
            bool  solid;
            bool  used;
        };

        Cell* Find(DWORD key, bool& found);
        Cell* Measured(float x, float z);
        void  Clear();
        void  Measure(float x, float z, Cell& cell);

        Cell mCells[CLEARANCE_TABLE_SIZE];
        int  mCount;
        int  mNewCells;
    };

    RoadIndex            gRoads;
    const TrackPathZone* gPlayerBridge = NULL;
    RoadSurfaces gRoadSurfaces;
    Course       gCourse;
    ClearanceMap gClearance;

    HeliSheetCoordinate MakeProbe();

    __declspec(align(16)) HeliSheetCoordinate gProbe = MakeProbe();

    bool  GameIgnoresDuringSkidHit(const AIActionHeliPursuit* pursuit);
    bool  IsSwooping(const AIActionHeliPursuit* pursuit);
    bool  IsOutOfRange(bool farFromTarget, const UMath::Vector3& heliPosition);
    bool  FindGround(float x, float z, float top, float& groundY, bool& paved, const TrackPathZone** bridge);
    bool  PlayerOnBridgeAt(float x, float z);
    float AboveBridge(float top, float bridgeElevation);
    bool  SurfaceBelow(float x, float z, float top, float& groundY);
    void  MeasureLowPoints(const IRigidBody* heli, float& levelLift, float& tiltedLift);
    void  RaiseAlong(float& highest, float x, float z, const UMath::Vector3& from, float dirX, float dirZ, float length);
    float Clearance(const HeliSheetCoordinate* coord, float x, float z);
    float CellCentre(int cell);

}

HeliSheet::Settings HeliSheet::sSettings;

HeliSheet::Settings::Settings()
    : HeliSheet(true),
      IgnoreHeliSheetDistance(200.0f) {
}

void HeliSheetCoordinate::InstallPatches() {
    Patch::Begin();
    Patch::RedirectCall(kFilterHeliAltitude, MethodAddress(&HeliSheetCoordinate::RoadElevation));
    Patch::RedirectCall(kCheckHeliSheetSmoothing, MethodAddress(&HeliSheetCoordinate::RoadElevation));
    Patch::RedirectCall(kCheckHeliSheetWorkingPos, MethodAddress(&HeliSheetCoordinate::RoadElevation));
    Patch::Commit();
}

bool TrackPathZone::IsPointInside(const bVector2& point) const {
    if (point.x < BBoxMin.x || point.x > BBoxMax.x || point.y < BBoxMin.y || point.y > BBoxMax.y) return false;
    int count = NumPoints > MOST_ZONE_POINTS ? MOST_ZONE_POINTS : NumPoints;
    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        const bVector2& a = Points[i];
        const bVector2& b = Points[j];
        if ((a.y > point.y) != (b.y > point.y) && point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x) inside = !inside;
    }
    return inside;
}

const TrackPathZone* TrackPathManager::FindBridge(const bVector2& point) const {
    if (pZones == NULL) return NULL;
    for (int i = 0; i < BRIDGE_ZONE_COUNT; ++i) {
        int type = kBridgeZones[i];
        const ZoneInfo& info = ZoneInfoTable[type];
        if (info.NumZones <= 0 || info.pFirstZone == NULL) continue;
        for (const TrackPathZone* zone = info.pFirstZone; zone < info.pLastZone; zone = zone->GetMemoryImageNext()) {
            if (zone->MemoryImageSize <= 0) break;
            if (zone->Type == type && zone->IsPointInside(point)) return zone;
        }
    }
    return NULL;
}

const TrackPathZone* TrackPathManager::FindCover(const bVector2& point, float height) const {
    if (pZones == NULL) return NULL;
    for (int i = 0; i < COVER_ZONE_COUNT; ++i) {
        int type = kCoverZones[i];
        const ZoneInfo& info = ZoneInfoTable[type];
        if (info.NumZones <= 0 || info.pFirstZone == NULL) continue;
        for (const TrackPathZone* zone = info.pFirstZone; zone < info.pLastZone; zone = zone->GetMemoryImageNext()) {
            if (zone->MemoryImageSize <= 0) break;
            if (zone->Type == type && height < zone->Elevation && zone->IsPointInside(point)) return zone;
        }
    }
    return NULL;
}

float WWorldPos::HeightAt(float x, float z) const {
    UMath::Vector3 u = fFace.fPt1 - fFace.fPt0;
    UMath::Vector3 v = fFace.fPt2 - fFace.fPt0;
    float nx = u.y * v.z - u.z * v.y;
    float ny = u.z * v.x - u.x * v.z;
    float nz = u.x * v.y - u.y * v.x;
    if (fabsf(ny) < 1 / (float)FLAT_FACE_LIMIT) return Larger(fFace.fPt0.y, Larger(fFace.fPt1.y, fFace.fPt2.y));
    return fFace.fPt0.y - (nx * (x - fFace.fPt0.x) + nz * (z - fFace.fPt0.z)) / ny;
}

float HeliSheetCoordinate::GetElevation(const bVector2& point, bVector3* normalOut, bool* pointValid) {
    return ((GetElevationMethod)HELI_SHEET_GET_ELEVATION)(this, &point, normalOut, pointValid);
}

float HeliSheetCoordinate::RoadElevation(const bVector2& point, bVector3* normalOut, bool* pointValid) {
    float elevation = GetElevation(point, normalOut, pointValid);
    if (pointValid != NULL && !*pointValid) return elevation;

    float x = -point.y;
    float z = point.x;
    float groundY = 0;
    bool paved = false;
    const TrackPathZone* bridge = NULL;
    bool onGround = HeliSheet::GroundUnder(x, z, elevation, groundY, paved, &bridge);
    if (onGround && bridge != NULL && !PlayerOnBridgeAt(x, z)) {
        groundY = AboveBridge(groundY, bridge->Elevation);
        paved = true;
    }
    float clearance = Clearance(this, x, z);
    float groundFloor = onGround ? Smaller(elevation, groundY + (paved ? ROAD_CLEARANCE : GROUND_CLEARANCE)) : elevation;
    float lowest = Larger(groundFloor, clearance);

    if (lowest < elevation && normalOut != NULL) {
        normalOut->x = 0;
        normalOut->y = 0;
        normalOut->z = 1;
        normalOut->w = 0;
    }
    return lowest;
}

bool HeliSheet::HookCheckHeliSheetPush() {
    return Detour::Install(CHECK_HELI_SHEET_PUSH, kCheckHeliSheetPushCode, sizeof(kCheckHeliSheetPushCode), &CheckHeliSheetPushEntry);
}

void __cdecl HeliSheet::CheckHeliSheetPushEntry(Detour::Registers* registers) {
    IRigidBody* rigidBody = (IRigidBody*)registers->esi;
    DWORD pushedAddress = 0;
    UMath::Vector3 pushed = UMath::Vector(0, 0, 0);
    if (rigidBody == NULL || !Memory::Read(registers->esp + PUSHED_VELOCITY_ARGUMENT, &pushedAddress, sizeof(pushedAddress))
        || !Memory::Read(pushedAddress, &pushed, sizeof(pushed)))
        return;

    UMath::Vector3 soft = rigidBody->GetLinearVelocity();
    if (!UMath::IsFinite(soft) || !UMath::IsFinite(pushed)) return;
    float limit = SimpleChopper::ClimbLimit(rigidBody->GetPosition(), sqrtf(soft.x * soft.x + soft.z * soft.z));
    soft.y = Larger(soft.y, Smaller(pushed.y, limit));
    Memory::WriteData(pushedAddress, &soft, sizeof(soft));
}

bool HeliSheet::GroundUnder(float x, float z, float top, float& groundY, bool& paved, const TrackPathZone** bridge) {
    return FindGround(x, z, top, groundY, paved, bridge);
}

bool HeliSheet::InsideSolid(const UMath::Vector3& point) {
    float top = 0;
    return gClearance.Top(point.x, point.z, top) && point.y < top - INSIDE_TOLERANCE;
}

bool HeliSheet::UnderCover(const UMath::Vector3& point, bool roofs) {
    if (TrackPathManager::Get().FindCover(MakePoint(point.z, -point.x), point.y) != NULL) return true;
    float top = 0;
    return roofs && gClearance.Top(point.x, point.z, top) && top > point.y + ROOF_ABOVE_CAR;
}

float HeliSheet::GroundLift(const IRigidBody* heli) {
    float levelLift = 0;
    float tiltedLift = 0;
    MeasureLowPoints(heli, levelLift, tiltedLift);
    return levelLift;
}

bool HeliSheet::TiltedNearGround(const IRigidBody* heli) {
    float levelLift = 0;
    float tiltedLift = 0;
    MeasureLowPoints(heli, levelLift, tiltedLift);
    return tiltedLift > 0 && tiltedLift > levelLift;
}

void HeliSheet::Update(State& state, const AIVehicleHelicopter& heli, const UMath::Vector3& heliPosition, const UMath::Vector3& heliVelocity,
                       const AIActionHeliPursuit* pursuit) {
    state.farFromTarget = IsOutOfRange(state.farFromTarget, heliPosition);
    *(bool*)B_IGNORE_HELI_SHEET = !sSettings.HeliSheet || state.farFromTarget || GameIgnoresDuringSkidHit(pursuit) || IsSwooping(pursuit);

    gCourse.heli = &heli;
    gCourse.position = heliPosition;
    gCourse.velocity = heliVelocity;
    gCourse.dest = heli.mDest + heli.mDestinationVelocity * Seconds(LEAD_POSITION_MS);
    gCourse.skidHitting = pursuit != NULL && pursuit->IsSkidHitting();
    gClearance.NewUpdate();

    IRigidBody* player = GetLocalPlayerRigidBody();
    UMath::Vector3 car = player != NULL ? player->GetPosition() : UMath::Vector(0, 0, 0);
    gPlayerBridge = player != NULL ? TrackPathManager::Get().FindBridge(MakePoint(car.z, -car.x)) : NULL;
    if (gPlayerBridge != NULL && !(car.y > gPlayerBridge->Elevation)) gPlayerBridge = NULL;
}

namespace {

    bool ClearanceMap::Height(float x, float z, float& height) {
        bool solid = false;
        return Height(x, z, height, solid);
    }

    bool ClearanceMap::Height(float x, float z, float& height, bool& solid) {
        Cell* cell = Measured(x, z);
        if (cell == NULL) return false;
        height = cell->height;
        if (cell->solid && cell->bridge && !PlayerOnBridgeAt(x, z))
            height = Smaller(cell->sheet, AboveBridge(cell->top, cell->bridgeElevation) + ROAD_CLEARANCE);
        solid = cell->solid;
        return cell->known;
    }

    bool ClearanceMap::Top(float x, float z, float& top) {
        Cell* cell = Measured(x, z);
        if (cell == NULL || !cell->known || !cell->solid) return false;
        top = cell->top;
        return true;
    }

    ClearanceMap::Cell* ClearanceMap::Measured(float x, float z) {
        int cx = (int)floorf(x / CLEARANCE_CELL);
        int cz = (int)floorf(z / CLEARANCE_CELL);
        DWORD key = ((DWORD)cx & CELL_KEY_MASK) << 16 | ((DWORD)cz & CELL_KEY_MASK);
        bool found = false;
        Cell* cell = Find(key, found);
        if (found) return cell;
        if (mNewCells >= NEW_CELLS_PER_UPDATE) return NULL;
        ++mNewCells;
        if (mCount >= MOST_CLEARANCE_CELLS) {
            Clear();
            cell = Find(key, found);
        }
        cell->key = key;
        cell->used = true;
        ++mCount;
        Measure(CellCentre(cx), CellCentre(cz), *cell);
        return cell;
    }

    ClearanceMap::Cell* ClearanceMap::Find(DWORD key, bool& found) {
        DWORD slot = (key * CLEARANCE_HASH) >> (32 - CLEARANCE_TABLE_BITS);
        while (mCells[slot].used && mCells[slot].key != key) slot = (slot + 1) & (CLEARANCE_TABLE_SIZE - 1);
        found = mCells[slot].used;
        return &mCells[slot];
    }

    void ClearanceMap::Clear() {
        memset(mCells, 0, sizeof(mCells));
        mCount = 0;
    }

    void ClearanceMap::Measure(float x, float z, Cell& cell) {
        bool valid = false;
        float sheet = gProbe.GetElevation(MakePoint(z, -x), NULL, &valid);
        cell.height = 0;
        cell.top = 0;
        cell.sheet = sheet;
        cell.bridgeElevation = 0;
        cell.bridge = false;
        cell.known = valid;
        cell.solid = false;
        if (!valid) return;
        float groundY = 0;
        bool paved = false;
        const TrackPathZone* bridge = NULL;
        cell.solid = FindGround(x, z, sheet, groundY, paved, &bridge);
        cell.top = groundY;
        cell.bridge = cell.solid && bridge != NULL;
        cell.bridgeElevation = bridge != NULL ? bridge->Elevation : 0;
        cell.height = cell.solid ? Smaller(sheet, groundY + ROAD_CLEARANCE) : sheet;
    }

    HeliSheetCoordinate MakeProbe() {
        HeliSheetCoordinate probe;
        memset(&probe, 0, sizeof(probe));
        float elevation = STARTING_ELEVATION;
        memcpy(probe.mData, &elevation, sizeof(elevation));
        return probe;
    }

    bool GameIgnoresDuringSkidHit(const AIActionHeliPursuit* pursuit) {
        return pursuit != NULL && pursuit->IsSkidHitting() && !*(const bool*)NEVER_IGNORE_HELI_SHEET;
    }

    bool IsSwooping(const AIActionHeliPursuit* pursuit) {
        return pursuit != NULL && pursuit->IsStriking() && !pursuit->IsPerpUnderCover();
    }

    bool IsOutOfRange(bool farFromTarget, const UMath::Vector3& heliPosition) {
        float ignoreDistance = HeliSheet::sSettings.IgnoreHeliSheetDistance;
        IRigidBody* playerRigidBody = ignoreDistance > 0 ? GetLocalPlayerRigidBody() : NULL;
        if (playerRigidBody == NULL) return false;

        float distance = UMath::Distancexz(heliPosition, playerRigidBody->GetPosition());
        float limit = farFromTarget ? ignoreDistance * Fraction(RETURN_PERCENT) : ignoreDistance;
        return distance > limit;
    }

    bool FindGround(float x, float z, float top, float& groundY, bool& paved, const TrackPathZone** bridge) {
        if (bridge != NULL) *bridge = NULL;
        float roadY = 0;
        bool onRoad = gRoads.Under(x, z, roadY);
        paved = onRoad;

        WWorldPos ground(Metres(FACE_TOLERANCE_MM));
        ground.FindClosestFace(UMath::Vector(x, top + ABOVE_SHEET, z), false);
        if (ground.OnValidFace()) {
            paved = onRoad || gRoadSurfaces.Contains(ground.GetSurface());
            groundY = ground.HeightAt(x, z);
        } else {
            if (!onRoad) return false;
            groundY = roadY;
        }
        const TrackPathZone* cover = TrackPathManager::Get().FindCover(MakePoint(z, -x), groundY);
        if (cover != NULL && cover->Type == TrackPathManager::TRACK_PATH_ZONE_TUNNEL) return false;
        if (cover != NULL) groundY = Larger(groundY, cover->Elevation + ABOVE_COVER);
        if (bridge != NULL) *bridge = TrackPathManager::Get().FindBridge(MakePoint(z, -x));
        return true;
    }

    bool PlayerOnBridgeAt(float x, float z) {
        return gPlayerBridge != NULL && gPlayerBridge->IsPointInside(MakePoint(z, -x));
    }

    float AboveBridge(float top, float bridgeElevation) {
        return Larger(top, bridgeElevation) + ABOVE_BRIDGE;
    }

    bool SurfaceBelow(float x, float z, float top, float& groundY) {
        WWorldPos ground(Metres(FACE_TOLERANCE_MM));
        ground.FindClosestFace(UMath::Vector(x, top, z), false);
        if (ground.OnValidFace()) {
            groundY = ground.HeightAt(x, z);
            return groundY <= top;
        }
        return gRoads.Under(x, z, groundY) && groundY <= top;
    }

    void MeasureLowPoints(const IRigidBody* heli, float& levelLift, float& tiltedLift) {
        levelLift = 0;
        tiltedLift = 0;
        UMath::Vector3 position = heli->GetPosition();
        float top = position.y + Metres(PROBE_ABOVE_MM);
        float groundY = 0;
        if (!SurfaceBelow(position.x, position.z, top, groundY) || position.y - groundY > LOW_POINTS_CHECK_HEIGHT) return;

        UMath::Vector3 right = UMath::Vector(0, 0, 0);
        UMath::Vector3 up = UMath::Vector(0, 0, 0);
        UMath::Vector3 forward = UMath::Vector(0, 0, 0);
        heli->GetRightVector(right);
        heli->GetUpVector(up);
        heli->GetForwardVector(forward);

        for (int i = 0; i < LOW_POINT_COUNT; ++i) {
            UMath::Vector3 point = position + right * Metres(kLowPointsMm[i][0]) + up * Metres(kLowPointsMm[i][1]) + forward * Metres(kLowPointsMm[i][2]);
            if (!SurfaceBelow(point.x, point.z, top, groundY)) continue;
            float lowest = groundY + Metres(GROUND_MARGIN_MM);
            tiltedLift = Larger(tiltedLift, lowest - point.y);
            levelLift = Larger(levelLift, lowest - (position.y + Metres(kLowPointsMm[i][1])));
        }
    }

    void RaiseAlong(float& highest, float x, float z, const UMath::Vector3& from, float dirX, float dirZ, float length) {
        for (float step = 0; step <= length; step += LOOK_STEP) {
            float pointX = from.x + dirX * step;
            float pointZ = from.z + dirZ * step;
            float height = 0;
            if (!gClearance.Height(pointX, pointZ, height)) continue;
            float distance = sqrtf((pointX - x) * (pointX - x) + (pointZ - z) * (pointZ - z));
            highest = Larger(highest, height - Fraction(CLIMB_PERCENT) * Larger(0, distance - FULL_HEIGHT_DISTANCE));
        }
    }

    float Clearance(const HeliSheetCoordinate* coord, float x, float z) {
        float highest = -FLT_MAX;
        bool ownCourse = gCourse.heli != NULL && gCourse.heli->OwnsHeliSheetCoord(coord);
        bool skidHitting = ownCourse && gCourse.skidHitting;

        float roadY = 0;
        bool trafficFree = false;
        bool onRoad = gRoads.Under(x, z, roadY, trafficFree);
        if (!skidHitting || (onRoad && trafficFree)) {
            for (int i = 0; i < BODY_CLEARANCE_POINTS; ++i) {
                float angle = RADIANS_PER_TURN * (float)i / (float)BODY_CLEARANCE_POINTS;
                float height = 0;
                bool solid = false;
                if (gClearance.Height(x + BODY_CLEARANCE * cosf(angle), z + BODY_CLEARANCE * sinf(angle), height, solid) && solid)
                    highest = Larger(highest, height);
                if (gClearance.Height(x + CABIN_CLEARANCE * cosf(angle), z + CABIN_CLEARANCE * sinf(angle), height)) highest = Larger(highest, height);
            }
        }
        if (!ownCourse) return highest;

        float toDestX = gCourse.dest.x - gCourse.position.x;
        float toDestZ = gCourse.dest.z - gCourse.position.z;
        float destDistance = sqrtf(toDestX * toDestX + toDestZ * toDestZ);
        if (destDistance > LOOK_STEP)
            RaiseAlong(highest, x, z, gCourse.position, toDestX / destDistance, toDestZ / destDistance, Smaller(destDistance + PAST_DEST, LONGEST_DEST_LOOK));

        float speed = sqrtf(gCourse.velocity.x * gCourse.velocity.x + gCourse.velocity.z * gCourse.velocity.z);
        if (!skidHitting && speed > SLOWEST_LOOK_AHEAD)
            RaiseAlong(highest, x, z, gCourse.position, gCourse.velocity.x / speed, gCourse.velocity.z / speed,
                       Smaller(speed * Seconds(LOOK_AHEAD_MS), LONGEST_LOOK_AHEAD));
        return highest;
    }

    float CellCentre(int cell) {
        return ((float)cell + Fraction(HALF_CELL_PERCENT)) * CLEARANCE_CELL;
    }

}
