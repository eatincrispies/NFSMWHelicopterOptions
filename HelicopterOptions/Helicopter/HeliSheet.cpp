#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "HeliSheet.hpp"
#include "AIActionHeliPursuit.hpp"

namespace {

    constexpr uintptr_t kbIgnoreHeliSheet     = 0x0090D621u;
    constexpr uintptr_t kNeverIgnoreHeliSheet = 0x008EB1F4u;
    constexpr float     kReturnFraction       = 0.8f;
    constexpr float     kRoadClearance        = 1.0f;
    constexpr float     kFaceTolerance        = 0.025f;
    constexpr float     kAboveSheet           = 1.0f;
    constexpr float     kRoadPieceLength      = 4.0f;
    constexpr int       kMostRoadPieces       = 48;
    constexpr float     kRoadCell             = 32.0f;
    constexpr float     kNarrowestRoad        = 4.0f;
    constexpr unsigned  kMostRoadSegments     = 0xFFFFu;

    static_assert(sizeof(bVector3) == 0x10, "bVector3");
    static_assert(sizeof(WRoadLane) == 0x4, "WRoadLane");
    static_assert(sizeof(WRoadProfile) == 0x40, "WRoadProfile");
    static_assert(offsetof(WRoadSegment, vEndHandle) == 0x10, "WRoadSegment::vEndHandle");
    static_assert(offsetof(WRoadSegment, vStartHandle) == 0x13, "WRoadSegment::vStartHandle");
    static_assert(sizeof(WRoadSegment) == 0x16, "WRoadSegment");
    static_assert(offsetof(WRoadNode, fProfileIndex) == 0xE, "WRoadNode::fProfileIndex");
    static_assert(offsetof(WRoadNode, fSegmentIndex) == 0x12, "WRoadNode::fSegmentIndex");
    static_assert(sizeof(WRoadNode) == 0x20, "WRoadNode");
    static_assert(sizeof(HeliSheetCoordinate) == 0x40, "HeliSheetCoordinate");
    static_assert(offsetof(WCollisionTri, fPt1) == 0x10, "WCollisionTri::fPt1");
    static_assert(offsetof(WCollisionTri, fPt2) == 0x20, "WCollisionTri::fPt2");
    static_assert(sizeof(WCollisionTri) == 0x30, "WCollisionTri");
    static_assert(offsetof(WWorldPos, fFaceBits) == 0x30, "WWorldPos::fFaceValid");
    static_assert(offsetof(WWorldPos, fYOffset) == 0x34, "WWorldPos::fYOffset");
    static_assert(offsetof(WWorldPos, fSurface) == 0x38, "WWorldPos::fSurface");
    static_assert(sizeof(WWorldPos) == 0x3C, "WWorldPos");

    constexpr uint32_t kRoadSurfaces[] = {
        0x19DB2F1Eu,
        0x35187254u,
        0x6CA26F9Bu,
        0x3830BABBu,
        0x02FC0FC9u,
        0xBA404853u,
        0x52E42012u,
    };
    constexpr size_t   kNumRoadSurfaces = sizeof(kRoadSurfaces) / sizeof(kRoadSurfaces[0]);
    constexpr uint32_t kDefaultSurface = 0u;

    bool gFarFromTarget = false;
    bool gIgnoring = false;

#if defined(_DEBUG)
    bool          gLoggedIgnore = false;
    bool          gLoggedReady = false;
    unsigned long gLastFloorLogMs = 0;
#endif

    bool& bIgnoreHeliSheet() {
        return Game::Global<bool>(kbIgnoreHeliSheet);
    }

    bool NeverIgnoreHeliSheet() {
        return Game::Global<bool>(kNeverIgnoreHeliSheet);
    }

    bool GameIgnoresDuringSkidHit(const AIActionHeliPursuit* pursuit) {
        return pursuit != nullptr && pursuit->IsSkidHitting() && !NeverIgnoreHeliSheet();
    }

    bool IsOutOfRange(const UMath::Vector3& heliPosition) {
        const float ignoreDistance = HeliSheet::sSettings.IgnoreHeliSheetDistance;
        IRigidBody* playerRigidBody = ignoreDistance > 0.0f ? GetLocalPlayerRigidBody() : nullptr;
        if (playerRigidBody == nullptr) return false;

        const float distance = UMath::Distancexz(heliPosition, playerRigidBody->GetPosition());
        const float limit = gFarFromTarget ? ignoreDistance * kReturnFraction : ignoreDistance;
        const bool  outOfRange = distance > limit;
        if (outOfRange != gFarFromTarget)
            Log::Info("HeliSheet  dXZ=%.2f %s %.2f  IgnoreHeliSheetDistance=%g return=%g  far %d -> %d", distance, outOfRange ? ">" : "<=", limit,
                      ignoreDistance, ignoreDistance * kReturnFraction, gFarFromTarget, outOfRange);
        return outOfRange;
    }

    struct RoadPiece {
        float x0, y0, z0, half0;
        float x1, y1, z1, half1;
    };

    class RoadIndex {
      public:
        bool Under(float x, float z, float& roadY) {
            if (!Ready()) return false;
            const int cx = static_cast<int>(std::floor((x - mMinX) / kRoadCell));
            const int cz = static_cast<int>(std::floor((z - mMinZ) / kRoadCell));
            if (cx < 0 || cz < 0 || cx >= mCellsX || cz >= mCellsZ) return false;

            const int cell = cz * mCellsX + cx;
            bool found = false;
            for (uint32_t i = mCellStart[cell]; i < mCellStart[cell + 1]; ++i) {
                const RoadPiece& piece = mPieces[mCellPieces[i]];
                const float dx = piece.x1 - piece.x0;
                const float dz = piece.z1 - piece.z0;
                const float lengthSq = dx * dx + dz * dz;
                float t = lengthSq > 0.0f ? ((x - piece.x0) * dx + (z - piece.z0) * dz) / lengthSq : 0.0f;
                t = std::min(1.0f, std::max(0.0f, t));
                const float offX = piece.x0 + dx * t - x;
                const float offZ = piece.z0 + dz * t - z;
                const float half = piece.half0 + (piece.half1 - piece.half0) * t;
                if (offX * offX + offZ * offZ > half * half) continue;

                const float y = piece.y0 + (piece.y1 - piece.y0) * t;
                if (!found || y > roadY) roadY = y;
                found = true;
            }
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
            if (profiles == nullptr || node.fProfileIndex < 0 || static_cast<unsigned>(node.fProfileIndex) >= WRoadNetwork::NumProfiles())
                return kNarrowestRoad;

            const WRoadProfile& profile = profiles[node.fProfileIndex];
            const int zones = std::min<int>(profile.fNumZones, 15);
            float half = 0.0f;
            for (int i = 0; i < zones; ++i)
                half = std::max(half, std::fabs(profile.mLanes[i].GetOffset()) + std::fabs(profile.mLanes[i].GetWidth()) * 0.5f);
            return half > 0.0f ? half : kNarrowestRoad;
        }

        void Build() {
            mSegments = WRoadNetwork::Segments();
            mNumSegments = WRoadNetwork::NumSegments();
            mPieces.clear();
            mCellStart.clear();
            mCellPieces.clear();

            const WRoadNode* nodes = WRoadNetwork::Nodes();
            const unsigned   numNodes = WRoadNetwork::NumNodes();
            if (mNumSegments == 0 || mNumSegments > kMostRoadSegments) {
                Log::Warn("WRoadNetwork  fNumSegments[0x%08X]=%u, no road floor", WRoadNetwork::kfNumSegments, mNumSegments);
                return;
            }

            for (unsigned s = 0; s < mNumSegments; ++s) {
                const WRoadSegment& segment = mSegments[s];
                if (segment.fNodeIndex[0] >= numNodes || segment.fNodeIndex[1] >= numNodes) continue;
                const WRoadNode& start = nodes[segment.fNodeIndex[0]];
                const WRoadNode& end = nodes[segment.fNodeIndex[1]];
                const float startHalf = HalfWidth(start);
                const float endHalf = HalfWidth(end);

                const UMath::Vector3 p0 = start.fPosition;
                const UMath::Vector3 p3 = end.fPosition;
                UMath::Vector3 p1 = p0;
                UMath::Vector3 p2 = p3;
                int steps = 1;
                if (segment.IsCurved()) {
                    p1 = p0 + segment.GetStartControl();
                    p2 = p3 + segment.GetEndControl();
                    const float chord = UMath::Distancexz(p0, p3);
                    steps = std::min(kMostRoadPieces, std::max(1, static_cast<int>(std::ceil(chord / kRoadPieceLength))));
                }

                UMath::Vector3 previous = p0;
                float previousHalf = startHalf;
                for (int k = 1; k <= steps; ++k) {
                    const float t = static_cast<float>(k) / static_cast<float>(steps);
                    const float u = 1.0f - t;
                    const UMath::Vector3 point = p0 * (u * u * u) + p1 * (3.0f * u * u * t) + p2 * (3.0f * u * t * t) + p3 * (t * t * t);
                    const float half = startHalf + (endHalf - startHalf) * t;
                    mPieces.push_back({ previous.x, previous.y, previous.z, previousHalf, point.x, point.y, point.z, half });
                    previous = point;
                    previousHalf = half;
                }
            }
            if (mPieces.empty()) return;

            float minX = mPieces[0].x0;
            float maxX = minX;
            float minZ = mPieces[0].z0;
            float maxZ = minZ;
            for (const RoadPiece& piece : mPieces) {
                const float half = std::max(piece.half0, piece.half1);
                minX = std::min(minX, std::min(piece.x0, piece.x1) - half);
                maxX = std::max(maxX, std::max(piece.x0, piece.x1) + half);
                minZ = std::min(minZ, std::min(piece.z0, piece.z1) - half);
                maxZ = std::max(maxZ, std::max(piece.z0, piece.z1) + half);
            }
            mMinX = minX;
            mMinZ = minZ;
            mCellsX = static_cast<int>((maxX - minX) / kRoadCell) + 1;
            mCellsZ = static_cast<int>((maxZ - minZ) / kRoadCell) + 1;

            std::vector<uint32_t> counts(static_cast<size_t>(mCellsX) * mCellsZ + 1, 0u);
            for (int pass = 0; pass < 2; ++pass) {
                for (uint32_t index = 0; index < mPieces.size(); ++index) {
                    const RoadPiece& piece = mPieces[index];
                    const float half = std::max(piece.half0, piece.half1);
                    const int x0 = static_cast<int>((std::min(piece.x0, piece.x1) - half - mMinX) / kRoadCell);
                    const int x1 = static_cast<int>((std::max(piece.x0, piece.x1) + half - mMinX) / kRoadCell);
                    const int z0 = static_cast<int>((std::min(piece.z0, piece.z1) - half - mMinZ) / kRoadCell);
                    const int z1 = static_cast<int>((std::max(piece.z0, piece.z1) + half - mMinZ) / kRoadCell);
                    for (int cz = std::max(0, z0); cz <= std::min(mCellsZ - 1, z1); ++cz)
                        for (int cx = std::max(0, x0); cx <= std::min(mCellsX - 1, x1); ++cx) {
                            const size_t cell = static_cast<size_t>(cz) * mCellsX + cx;
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

            Log::Info("WRoadNetwork  fSegments[0x%08X]=0x%p fNumSegments=%u fNumNodes=%u fNumProfiles=%u -> %u road pieces, %d x %d cells of %g m, %u entries",
                      WRoadNetwork::kfSegments, mSegments, mNumSegments, numNodes, WRoadNetwork::NumProfiles(), static_cast<unsigned>(mPieces.size()),
                      mCellsX, mCellsZ, kRoadCell, static_cast<unsigned>(mCellPieces.size()));
        }

        std::vector<RoadPiece> mPieces;
        std::vector<uint32_t>  mCellStart;
        std::vector<uint32_t>  mCellPieces;
        const WRoadSegment*    mSegments = nullptr;
        unsigned               mNumSegments = 0;
        float                  mMinX = 0.0f;
        float                  mMinZ = 0.0f;
        int                    mCellsX = 0;
        int                    mCellsZ = 0;
    };

    RoadIndex gRoads;

    class RoadSurfaces {
      public:
        bool Contains(const void* surface) {
            if (surface == nullptr || !Ready()) return false;
            for (const void* road : mSurfaces)
                if (road == surface) return true;
            return false;
        }

      private:
        bool Ready() {
            if (mReady) return true;
            const void* fallback = SimSurface::Lookup(kDefaultSurface);
            int found = 0;
            for (size_t i = 0; i < kNumRoadSurfaces; ++i) {
                const void* surface = SimSurface::Lookup(kRoadSurfaces[i]);
                mSurfaces[i] = surface != fallback ? surface : nullptr;
                if (mSurfaces[i] != nullptr) ++found;
            }
            mReady = found > 0;
            Log::Info("SimSurface  %d of %u road surfaces found, default 0x%p", found, static_cast<unsigned>(kNumRoadSurfaces), fallback);
            return mReady;
        }

        const void* mSurfaces[kNumRoadSurfaces] = {};
        bool        mReady = false;
    };

    RoadSurfaces gRoadSurfaces;

#if defined(_DEBUG)
    void LogRoadFloor(const bVector2& point, float elevation, float roadY, float floor) {
        const unsigned long now = GetTickCount();
        if (now - gLastFloorLogMs < 1000) return;
        gLastFloorLogMs = now;
        const IRigidBody* player = GetLocalPlayerRigidBody();
        const float carY = player != nullptr ? player->GetPosition().y : 0.0f;
        Log::Info("HeliSheet  at (%.1f, %.1f)  sheet=%.2f ground=%.2f car=%.2f  sheet is %.1f m above the ground -> %s %.2f", -point.y, point.x,
                  elevation, roadY, carY, elevation - roadY, floor < elevation ? "lowered to" : "kept", std::min(floor, elevation));
    }
#else
    void LogRoadFloor(const bVector2&, float, float, float) {}
#endif

}

void HeliSheetCoordinate::InstallPatches() {
    Patch::Begin("HeliSheetCoordinate::GetElevation road floor");
    Patch::RedirectCall("AIVehicleHelicopter::FilterHeliAltitude", kFilterHeliAltitude, Game::MethodAddress(&HeliSheetCoordinate::RoadElevation));
    Patch::RedirectCall("AIVehicleHelicopter::CheckHeliSheet mThirdHeliSheetCoord", kCheckHeliSheetSmoothing,
                        Game::MethodAddress(&HeliSheetCoordinate::RoadElevation));
    Patch::RedirectCall("AIVehicleHelicopter::CheckHeliSheet mSecondaryHeliSheetCoord", kCheckHeliSheetWorkingPos,
                        Game::MethodAddress(&HeliSheetCoordinate::RoadElevation));
    Patch::Commit();
}

float WWorldPos::HeightAt(float x, float z) const {
    const UMath::Vector3 u = fFace.fPt1 - fFace.fPt0;
    const UMath::Vector3 v = fFace.fPt2 - fFace.fPt0;
    const float nx = u.y * v.z - u.z * v.y;
    const float ny = u.z * v.x - u.x * v.z;
    const float nz = u.x * v.y - u.y * v.x;
    if (std::fabs(ny) < 1e-4f) return std::max(fFace.fPt0.y, std::max(fFace.fPt1.y, fFace.fPt2.y));
    return fFace.fPt0.y - (nx * (x - fFace.fPt0.x) + nz * (z - fFace.fPt0.z)) / ny;
}

float HeliSheetCoordinate::GetElevation(const bVector2& point, bVector3* normalOut, bool* pointValid) {
    return Game::ThisCall<float>(kGetElevation, this, &point, normalOut, pointValid);
}

float HeliSheetCoordinate::RoadElevation(const bVector2& point, bVector3* normalOut, bool* pointValid) {
    const float elevation = GetElevation(point, normalOut, pointValid);
    if (pointValid != nullptr && !*pointValid) return elevation;

    float groundY = 0.0f;
    if (!HeliSheet::GroundUnder(-point.y, point.x, elevation, groundY)) return elevation;
    const float floor = groundY + kRoadClearance;
    LogRoadFloor(point, elevation, groundY, floor);
    if (floor >= elevation) return elevation;

    if (normalOut != nullptr) *normalOut = { 0.0f, 0.0f, 1.0f, 0.0f };
    return floor;
}

bool HeliSheet::GroundUnder(float x, float z, float top, float& groundY) {
    float roadY = 0.0f;
    const bool onRoad = gRoads.Under(x, z, roadY);

    WWorldPos ground(kFaceTolerance);
    ground.FindClosestFace({ x, top + kAboveSheet, z }, false);
    if (ground.OnValidFace()) {
        if (!onRoad && !gRoadSurfaces.Contains(ground.GetSurface())) return false;
        groundY = ground.HeightAt(x, z);
        return true;
    }
    if (!onRoad) return false;
    groundY = roadY;
    return true;
}

void HeliSheet::BeginHelicopter() {
    gFarFromTarget = false;
}

void HeliSheet::Update(const UMath::Vector3& heliPosition, const AIActionHeliPursuit* pursuit) {
    gFarFromTarget = IsOutOfRange(heliPosition);

    const bool ignore = !sSettings.HeliSheet || gFarFromTarget;
    if (ignore || gIgnoring) {
        bIgnoreHeliSheet() = ignore || GameIgnoresDuringSkidHit(pursuit);
        gIgnoring = ignore;
    }
}

#if defined(_DEBUG)
void HeliSheet::LogLive(bool full) {
    const bool ignore = bIgnoreHeliSheet();
    if (!gLoggedReady || ignore != gLoggedIgnore) {
        Log::Info("HeliSheet  bIgnoreHeliSheet[0x%08X] %d -> %d  NeverIgnoreHeliSheet[0x%08X]=%d far=%d forced=%d", kbIgnoreHeliSheet, gLoggedIgnore,
                  ignore, kNeverIgnoreHeliSheet, NeverIgnoreHeliSheet(), gFarFromTarget, gIgnoring);
        gLoggedIgnore = ignore;
        gLoggedReady = true;
    }
    if (full)
        Log::Info("HeliSheet  bIgnoreHeliSheet[0x%08X]=%d NeverIgnoreHeliSheet[0x%08X]=%d  HeliSheet=%d IgnoreHeliSheetDistance=%g far=%d forced=%d",
                  kbIgnoreHeliSheet, ignore, kNeverIgnoreHeliSheet, NeverIgnoreHeliSheet(), sSettings.HeliSheet, sSettings.IgnoreHeliSheetDistance,
                  gFarFromTarget, gIgnoring);
}
#endif
