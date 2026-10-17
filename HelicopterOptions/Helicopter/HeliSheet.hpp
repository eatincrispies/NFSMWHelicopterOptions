#pragma once
#include "../../dllmain.hpp"

class AIActionHeliPursuit;

struct bVector2 {
    float x, y;
};

struct bVector3 {
    float x, y, z, w;
};

struct WRoadLane {
    float GetWidth() const {
        return static_cast<float>(static_cast<int32_t>(nBits << 14) >> 18) * (100.0f / 8191.0f);
    }

    float GetOffset() const {
        return static_cast<float>(static_cast<int32_t>(nBits) >> 18) * (100.0f / 8191.0f);
    }

    uint32_t nBits;
};

struct WRoadProfile {
    unsigned char fNumZones;
    unsigned char fMiddleZone;
    unsigned char nPadding[2];
    WRoadLane     mLanes[15];
};

struct WRoadSegment {
    static constexpr unsigned short kRoadSegmentCurved = 1 << 8;

    bool IsCurved() const {
        return (fFlags & kRoadSegmentCurved) != 0;
    }

    UMath::Vector3 GetStartControl() const {
        const float scale = static_cast<float>(fStartHandleLength) * (500.0f / (127.0f * 65535.0f));
        return { scale * vStartHandle[0], scale * vStartHandle[1], scale * vStartHandle[2] };
    }

    UMath::Vector3 GetEndControl() const {
        const float scale = static_cast<float>(fEndHandleLength) * (500.0f / (127.0f * 65535.0f));
        return { scale * vEndHandle[0], scale * vEndHandle[1], scale * vEndHandle[2] };
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
    static constexpr uintptr_t kFindClosestFace = 0x00789190u;

    explicit WWorldPos(float yOffset) : fYOffset(yOffset) {}

    bool FindClosestFace(const UMath::Vector3& pt, bool quitIfOnSameFace) {
        return Game::ThisCall<bool>(kFindClosestFace, this, &pt, quitIfOnSameFace);
    }

    bool OnValidFace() const {
        return (fFaceBits & 1u) != 0;
    }

    const void* GetSurface() const {
        return fSurface;
    }

    float HeightAt(float x, float z) const;

    WCollisionTri fFace = {};
    unsigned int  fFaceBits = 0;
    float         fYOffset;
    const void*   fSurface = nullptr;
};

class SimSurface {
  public:
    static constexpr uintptr_t kLookup = 0x006E95E0u;

    static const void* Lookup(uint32_t nameHash) {
        return Game::Call<const void*>(kLookup, &nameHash);
    }
};

class WRoadNetwork {
  public:
    static constexpr uintptr_t kfValid       = 0x009B3828u;
    static constexpr uintptr_t kfNumSegments = 0x009B3874u;
    static constexpr uintptr_t kfNumNodes    = 0x009B387Cu;
    static constexpr uintptr_t kfNumProfiles = 0x009B3884u;
    static constexpr uintptr_t kfProfiles    = 0x009B38B8u;
    static constexpr uintptr_t kfNodes       = 0x009B38BCu;
    static constexpr uintptr_t kfSegments    = 0x009B38C0u;

    static bool IsValid() {
        return Game::Global<bool>(kfValid) && Game::Global<const WRoadSegment*>(kfSegments) != nullptr && Game::Global<const WRoadNode*>(kfNodes) != nullptr;
    }

    static const WRoadSegment* Segments() {
        return Game::Global<const WRoadSegment*>(kfSegments);
    }

    static unsigned NumSegments() {
        return Game::Global<unsigned>(kfNumSegments);
    }

    static const WRoadNode* Nodes() {
        return Game::Global<const WRoadNode*>(kfNodes);
    }

    static unsigned NumNodes() {
        return Game::Global<unsigned>(kfNumNodes);
    }

    static const WRoadProfile* Profiles() {
        return Game::Global<const WRoadProfile*>(kfProfiles);
    }

    static unsigned NumProfiles() {
        return Game::Global<unsigned>(kfNumProfiles);
    }
};

struct HeliSheetCoordinate {
    static constexpr uintptr_t       kGetElevation              = 0x00754290u;
    static constexpr Patch::CallSite kFilterHeliAltitude        = { 0x004099A6u, kGetElevation };
    static constexpr Patch::CallSite kCheckHeliSheetSmoothing   = { 0x00417514u, kGetElevation };
    static constexpr Patch::CallSite kCheckHeliSheetWorkingPos  = { 0x004175FCu, kGetElevation };

    static void InstallPatches();

    float GetElevation(const bVector2& point, bVector3* normalOut, bool* pointValid);
    float RoadElevation(const bVector2& point, bVector3* normalOut, bool* pointValid);

    unsigned char mData[0x40];
};

namespace HeliSheet {

    struct Settings {
        bool  HeliSheet               = true;
        float IgnoreHeliSheetDistance = 0.0f;
    };

    inline Settings sSettings;

    inline const Ini::Setting kIniSettings[] = {
        { "Helicopter:HeliSheet",               "HeliSheet",               nullptr,                            &sSettings.HeliSheet, 0.0f, 1.0f,    false },
        { "Helicopter:IgnoreHeliSheetDistance", "IgnoreHeliSheetDistance", &sSettings.IgnoreHeliSheetDistance, nullptr,              0.0f, 2000.0f, false },
    };

    void BeginHelicopter();
    void Update(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit);
    bool GroundUnder(float x, float z, float top, float& groundY);

#if defined(_DEBUG)
    void LogLive(bool full);
#else
    inline void LogLive(bool) {}
#endif

}
