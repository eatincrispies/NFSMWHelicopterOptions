#pragma once
#include "../../dllmain.hpp"

#define WWORLDPOS_FIND_CLOSEST_FACE      0x00789190
#define SIM_SURFACE_LOOKUP               0x006E95E0
#define TRACK_PATH_MANAGER               0x009AA540

#define ROAD_NETWORK_VALID               0x009B3828
#define ROAD_NETWORK_NUM_SEGMENTS        0x009B3874
#define ROAD_NETWORK_NUM_NODES           0x009B387C
#define ROAD_NETWORK_NUM_PROFILES        0x009B3884
#define ROAD_NETWORK_PROFILES            0x009B38B8
#define ROAD_NETWORK_NODES               0x009B38BC
#define ROAD_NETWORK_SEGMENTS            0x009B38C0

#define ROAD_SEGMENT_NO_TRAFFIC          (1 << 1)
#define ROAD_SEGMENT_CURVED              (1 << 8)
#define ROAD_LANE_SCALE                  (100.0f / 8191.0f)
#define ROAD_HANDLE_SCALE                (500.0f / (127.0f * 65535.0f))

class AIActionHeliPursuit;
class AIVehicleHelicopter;

typedef bool        (__thiscall* FindClosestFaceMethod)(void* position, const UMath::Vector3* point, bool quitIfOnSameFace);
typedef const void* (__cdecl* SurfaceLookupFunction)(const DWORD* nameHash);

struct bVector2 {
    float x, y;
};

struct bVector3 {
    float x, y, z, w;
};

struct __declspec(align(16)) bMatrix4 {
    float m[16];
};

inline bVector2 MakePoint(float x, float y) {
    bVector2 point;
    point.x = x;
    point.y = y;
    return point;
}

struct WRoadLane {
    float GetWidth() const {
        return (float)((int)(nBits << 14) >> 18) * ROAD_LANE_SCALE;
    }

    float GetOffset() const {
        return (float)((int)nBits >> 18) * ROAD_LANE_SCALE;
    }

    DWORD nBits;
};

struct WRoadProfile {
    unsigned char fNumZones;
    unsigned char fMiddleZone;
    unsigned char nPadding[2];
    WRoadLane     mLanes[15];
};

struct WRoadSegment {
    bool IsCurved() const {
        return (fFlags & ROAD_SEGMENT_CURVED) != 0;
    }

    bool IsTrafficAllowed() const {
        return (fFlags & ROAD_SEGMENT_NO_TRAFFIC) == 0;
    }

    UMath::Vector3 GetStartControl() const {
        float scale = (float)fStartHandleLength * ROAD_HANDLE_SCALE;
        return UMath::Vector(scale * vStartHandle[0], scale * vStartHandle[1], scale * vStartHandle[2]);
    }

    UMath::Vector3 GetEndControl() const {
        float scale = (float)fEndHandleLength * ROAD_HANDLE_SCALE;
        return UMath::Vector(scale * vEndHandle[0], scale * vEndHandle[1], scale * vEndHandle[2]);
    }

    unsigned short fNodeIndex[2];
    unsigned short nLength;
    short          fRoadID;
    short          fIndex;
    unsigned short fFlags;
    unsigned short fEndHandleLength;
    unsigned short fStartHandleLength;
    signed char    vEndHandle[3];
    signed char    vStartHandle[3];
};

struct WRoadNode {
    UMath::Vector3 fPosition;
    short          fIndex;
    short          fProfileIndex;
    unsigned char  fNumSegments;
    unsigned short fSegmentIndex[7];
};

struct WCollisionTri {
    UMath::Vector3 fPt0;
    const void*    fSurfaceRef;
    UMath::Vector3 fPt1;
    unsigned int   fFlags;
    UMath::Vector3 fPt2;
    unsigned short fSurface;
    unsigned short PAD;
};

class WWorldPos {
  public:
    explicit WWorldPos(float yOffset) : fFaceBits(0), fYOffset(yOffset), fSurface(NULL) {
        memset(&fFace, 0, sizeof(fFace));
    }

    bool FindClosestFace(const UMath::Vector3& pt, bool quitIfOnSameFace) {
        return ((FindClosestFaceMethod)WWORLDPOS_FIND_CLOSEST_FACE)(this, &pt, quitIfOnSameFace);
    }

    bool OnValidFace() const {
        return (fFaceBits & 1) != 0;
    }

    const void* GetSurface() const {
        return fSurface;
    }

    float HeightAt(float x, float z) const;

    WCollisionTri fFace;
    unsigned int  fFaceBits;
    float         fYOffset;
    const void*   fSurface;
};

class SimSurface {
  public:
    static const void* Lookup(DWORD nameHash) {
        return ((SurfaceLookupFunction)SIM_SURFACE_LOOKUP)(&nameHash);
    }
};

class WRoadNetwork {
  public:
    static bool IsValid() {
        return *(const bool*)ROAD_NETWORK_VALID && Segments() != NULL && Nodes() != NULL;
    }

    static const WRoadSegment* Segments() {
        return *(const WRoadSegment* const*)ROAD_NETWORK_SEGMENTS;
    }

    static unsigned NumSegments() {
        return *(const unsigned*)ROAD_NETWORK_NUM_SEGMENTS;
    }

    static const WRoadNode* Nodes() {
        return *(const WRoadNode* const*)ROAD_NETWORK_NODES;
    }

    static unsigned NumNodes() {
        return *(const unsigned*)ROAD_NETWORK_NUM_NODES;
    }

    static const WRoadProfile* Profiles() {
        return *(const WRoadProfile* const*)ROAD_NETWORK_PROFILES;
    }

    static unsigned NumProfiles() {
        return *(const unsigned*)ROAD_NETWORK_NUM_PROFILES;
    }
};

struct TrackPathZone {
    bool IsPointInside(const bVector2& point) const;

    const TrackPathZone* GetMemoryImageNext() const {
        return (const TrackPathZone*)((const char*)this + MemoryImageSize);
    }

    int         Type;
    bVector2    Position;
    bVector2    Direction;
    float       Elevation;
    signed char ZoneSource;
    signed char CachedIndex;
    short       VisitInfo;
    void*       pUserData;
    bVector2    BBoxMin;
    bVector2    BBoxMax;
    int         Data[4];
    short       NumPoints;
    short       MemoryImageSize;
    bVector2    Points[64];
};

class TrackPathManager {
  public:
    enum eTrackPathZoneType {
        TRACK_PATH_ZONE_TUNNEL         = 3,
        TRACK_PATH_ZONE_OVERPASS       = 4,
        TRACK_PATH_ZONE_OVERPASS_SMALL = 5,
        TRACK_PATH_ZONE_GARAGE         = 7,
        TRACK_PATH_ZONE_HIDDEN         = 8,
        TRACK_PATH_ZONE_DYNAMIC        = 10,
        NUM_TRACK_PATH_ZONES           = 15
    };

    struct ZoneInfo {
        int                  NumZones;
        const TrackPathZone* pFirstZone;
        const TrackPathZone* pLastZone;
        unsigned char        mCache[0x40];
    };

    static const TrackPathManager& Get() {
        return *(const TrackPathManager*)TRACK_PATH_MANAGER;
    }

    const TrackPathZone* FindCover(const bVector2& point, float height) const;
    const TrackPathZone* FindBridge(const bVector2& point) const;

    int                  NumZones;
    int                  SizeofZones;
    const TrackPathZone* pZones;
    ZoneInfo             ZoneInfoTable[NUM_TRACK_PATH_ZONES];
};

struct HeliSheetCoordinate {
    static void InstallPatches();

    float GetElevation(const bVector2& point, bVector3* normalOut, bool* pointValid);
    float RoadElevation(const bVector2& point, bVector3* normalOut, bool* pointValid);

    unsigned char mData[0x40];
};

namespace HeliSheet {

    struct Settings {
        Settings();

        bool  HeliSheet;
        float IgnoreHeliSheetDistance;
    };

    extern Settings sSettings;

    struct State {
        State() : farFromTarget(false) {}

        bool farFromTarget;
    };

    void Update(State& state, const AIVehicleHelicopter& heli, const UMath::Vector3& heliPosition, const UMath::Vector3& heliVelocity,
                const AIActionHeliPursuit* pursuit);
    bool GroundUnder(float x, float z, float top, float& groundY, bool& paved, const TrackPathZone** bridge);
    bool InsideSolid(const UMath::Vector3& point);
    float GroundLift(const IRigidBody* heli);
    bool  TiltedNearGround(const IRigidBody* heli);
    bool  HookCheckHeliSheetPush();
    void  __cdecl CheckHeliSheetPushEntry(Detour::Registers* registers);
    bool UnderCover(const UMath::Vector3& point, bool roofs);

}
