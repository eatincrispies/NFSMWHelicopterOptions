#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstring>
#include <cmath>
#include "HelicopterRegistry.h"
#include "../Config/Config.h"
#include "../Core/Addresses.h"
#include "../Core/Memory.h"
#include "../Core/Log.h"

namespace Systems { namespace Registry {

    namespace {
        constexpr int kMax = 8;
        Record   gRecords[kMax] = {};
        int      gCount = 0;
        uint32_t gNextId = 1;
        void*    gLastGlobal = nullptr;
        long     gLearnedDelta = 0;
        bool     gDeltaLearned = false;
        unsigned long gLastProxLogMs = 0;

        int SehReadOwner(void* obj, void** ownerOut) {
            __try {
                *ownerOut = *reinterpret_cast<void**>(
                    reinterpret_cast<uintptr_t>(obj) + Addr::Heli::kOwner);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        Record* FindByOwner(void* owner) {
            for (int i = 0; i < gCount; ++i)
                if (gRecords[i].mapped && gRecords[i].owner == owner) return &gRecords[i];
            return nullptr;
        }

        Record* NewRecord() {
            if (gCount < kMax) {
                Record& n = gRecords[gCount++];
                std::memset(&n, 0, sizeof(n));
                n.id = gNextId++;
                return &n;
            }
            for (int i = 0; i < gCount; ++i) {
                if (!gRecords[i].alive) {
                    std::memset(&gRecords[i], 0, sizeof(Record));
                    gRecords[i].id = gNextId++;
                    return &gRecords[i];
                }
            }
            return nullptr;
        }

        int MappedAlive() {
            int n = 0;
            for (int i = 0; i < gCount; ++i)
                if (gRecords[i].alive && gRecords[i].mapped) ++n;
            return n;
        }

        void ProximityCheck() {
            if (!gCfg.EnableProximityTelemetry) return;
            const unsigned long now = GetTickCount();
            if (now - gLastProxLogMs < static_cast<unsigned long>(gCfg.CollisionLogIntervalMs))
                return;
            for (int i = 0; i < gCount; ++i) {
                const Record& a = gRecords[i];
                if (!a.alive || !a.mapped || now - a.kinMs > 2000) continue;
                for (int j = i + 1; j < gCount; ++j) {
                    const Record& b = gRecords[j];
                    if (!b.alive || !b.mapped || now - b.kinMs > 2000) continue;

                    const float dx = a.pos[0] - b.pos[0];
                    const float dy = a.pos[1] - b.pos[1];
                    const float dz = a.pos[2] - b.pos[2];
                    const float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

                    // Linear closest-approach prediction within PredictSeconds.
                    const float rvx = a.vel[0] - b.vel[0];
                    const float rvy = a.vel[1] - b.vel[1];
                    const float rvz = a.vel[2] - b.vel[2];
                    const float rv2 = rvx*rvx + rvy*rvy + rvz*rvz;
                    float tStar = 0.0f;
                    if (rv2 > 0.01f) {
                        tStar = -(dx*rvx + dy*rvy + dz*rvz) / rv2;
                        if (tStar < 0.0f) tStar = 0.0f;
                        if (tStar > gCfg.CollisionPredictSeconds)
                            tStar = gCfg.CollisionPredictSeconds;
                    }
                    const float cx = dx + rvx*tStar, cy = dy + rvy*tStar, cz = dz + rvz*tStar;
                    const float dMin = std::sqrt(cx*cx + cy*cy + cz*cz);

                    if (dist < gCfg.CollisionWarnDistance || dMin < gCfg.CollisionWarnDistance) {
                        gLastProxLogMs = now;
                        Log::Warn("[ChopperCollision] registryId=%u <-> registryId=%u "
                                  "dist=%.1f m, predicted closest approach %.1f m in %.1fs. "
                                  "(No engine collision between helicopters is verified - "
                                  "expect phasing; this is research telemetry.)",
                                  a.id, b.id, dist, dMin, tStar);
                    }
                }
            }
        }
    }

    bool Enabled() {
        return gCfg.LogHeliLifecycle && (gCfg.EnableTelemetry || gCfg.EnableDispatchPatches);
    }

    void __cdecl OnCtor(void* ctorThis) {
        int aliveBefore = MappedAlive();

        for (int i = 0; i < gCount; ++i)
            if (gRecords[i].alive && gRecords[i].ctorPtr == ctorThis)
                gRecords[i].alive = false;   // address reused = old object gone

        Record* r = NewRecord();
        if (r) {
            r->ctorPtr = ctorThis;
            r->ctorTickMs = GetTickCount();
            r->alive = true;
            Log::Info("[Registry] helicopter #%u constructed (ctorPtr=%p, alive before: %d).",
                      r->id, ctorThis, aliveBefore);
        }
        if (aliveBefore >= 1) {
            Log::Warn("[Registry] MULTI-HELI EVENT: constructor fired while %d helicopter(s) "
                      "already alive - singleton bookkeeping now stale for the previous "
                      "instance(s). Research data.", aliveBefore);
        }
    }

    void NotifyDriving(void* aiThis, void* owner, void* rigidBody) {
        const unsigned long now = GetTickCount();

        Record* r = FindByOwner(owner);
        if (r) {
            r->lastSeenMs = now;
            r->alive = true;
            r->rigidBody = rigidBody;
            if (r->aiThis != aiThis) {
                void* globalHeli = nullptr;
                Memory::ReadPtr(Addr::kGlobalHeliVehicle, &globalHeli);
                Log::Info("AI this=%p owner=%p heli=%p registryId=%u (AI pointer changed from %p)",
                          aiThis, owner, globalHeli, r->id, r->aiThis);
                r->aiThis = aiThis;
            }
            return;
        }

        // Merge into a provisional constructor record:
        //   1. direct owner match  2. learned delta  3. single candidate
        Record* link = nullptr;
        const char* how = nullptr;

        for (int i = 0; i < gCount && !link; ++i) {
            Record& c = gRecords[i];
            if (c.alive && !c.mapped && c.ctorPtr) {
                void* ctorOwner = nullptr;
                if (SehReadOwner(c.ctorPtr, &ctorOwner) && ctorOwner == owner) {
                    link = &c; how = "ctorPtr+0x34 owner match";
                }
            }
        }
        if (!link && gDeltaLearned) {
            for (int i = 0; i < gCount && !link; ++i) {
                Record& c = gRecords[i];
                if (c.alive && !c.mapped && c.ctorPtr
                    && reinterpret_cast<intptr_t>(c.ctorPtr) + gLearnedDelta
                       == reinterpret_cast<intptr_t>(aiThis)) {
                    link = &c; how = "learned ctor/AI delta";
                }
            }
        }
        if (!link) {
            Record* only = nullptr;
            int candidates = 0;
            for (int i = 0; i < gCount; ++i) {
                Record& c = gRecords[i];
                if (c.alive && !c.mapped && c.ctorPtr && now - c.ctorTickMs < 60000) {
                    ++candidates;
                    only = &c;
                }
            }
            if (candidates == 1) {
                link = only; how = "single-candidate heuristic";
            } else if (candidates > 1) {
                Log::Warn("[Registry] merge failed: %d provisional constructor records are "
                          "candidates for owner=%p - creating a separate record. "
                          "(Multi-heli research data.)", candidates, owner);
            }
        }

        if (!link) {
            link = NewRecord();
            if (!link) return;
            link->alive = true;
            link->ctorTickMs = now;
            how = nullptr;
        }
        link->aiThis = aiThis;
        link->owner = owner;
        link->rigidBody = rigidBody;
        link->mapped = true;
        link->lastSeenMs = now;

        void* globalHeli = nullptr;
        Memory::ReadPtr(Addr::kGlobalHeliVehicle, &globalHeli);
        Log::Info("AI this=%p owner=%p heli=%p registryId=%u%s%s",
                  aiThis, owner, globalHeli, link->id,
                  how ? " (merged with constructor record via " : (link->ctorPtr ? "" :
                  " (no constructor record matched - helicopter predates the mod "
                  "or ctor hook inactive)"),
                  how ? how : "");
        if (link->ctorPtr && link->ctorPtr != aiThis) {
            const long delta = static_cast<long>(reinterpret_cast<intptr_t>(aiThis)
                                                 - reinterpret_cast<intptr_t>(link->ctorPtr));
            if (!gDeltaLearned) { gDeltaLearned = true; gLearnedDelta = delta; }
            Log::Info("[Registry] ctorPtr=%p aiThis=%p registryId=%u delta=%ld bytes "
                      "(pointer-relationship research data%s).",
                      link->ctorPtr, aiThis, link->id, delta,
                      how ? "" : "; delta NOT confirmed by owner match");
        }
    }

    void UpdateKinematics(void* owner, const float pos[3], const float vel[3]) {
        Record* r = FindByOwner(owner);
        if (!r) return;
        std::memcpy(r->pos, pos, sizeof(r->pos));
        std::memcpy(r->vel, vel, sizeof(r->vel));
        r->kinMs = GetTickCount();
    }

    void Tick() {
        if (!Enabled()) return;

        void* global = nullptr;
        if (Memory::ReadPtr(Addr::kGlobalHeliVehicle, &global)) {
            if (global != gLastGlobal) {
                Log::Info("[Registry] gHeliVehicle changed: %p -> %p.", gLastGlobal, global);
                gLastGlobal = global;
            }
        }

        const unsigned long now = GetTickCount();
        for (int i = 0; i < gCount; ++i) {
            Record& r = gRecords[i];
            if (!r.alive) continue;
            if (r.mapped && r.lastSeenMs != 0 && now - r.lastSeenMs > 30000) {
                r.alive = false;
                Log::Info("[Registry] helicopter registryId=%u (owner=%p) presumed gone "
                          "(no driving tick for 30 s; lifetime %.0f s).",
                          r.id, r.owner, (now - r.ctorTickMs) * 0.001);
            } else if (!r.mapped && now - r.ctorTickMs > 120000) {
                r.alive = false;
                Log::Info("[Registry] provisional constructor record registryId=%u "
                          "(ctorPtr=%p) expired unmerged after 120 s.", r.id, r.ctorPtr);
            }
        }

        ProximityCheck();
    }

    int AliveCount() {
        int mapped = 0, provisional = 0;
        for (int i = 0; i < gCount; ++i) {
            if (!gRecords[i].alive) continue;
            if (gRecords[i].mapped) ++mapped; else ++provisional;
        }
        return mapped > 0 ? mapped : provisional;
    }

} } // namespace Systems::Registry
