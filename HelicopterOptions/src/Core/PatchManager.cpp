#define _CRT_SECURE_NO_WARNINGS
#include <vector>
#include <cstring>
#include <cmath>
#include "PatchManager.h"
#include "Memory.h"
#include "Log.h"

namespace Patch {

    namespace {

        constexpr size_t kMaxPatchLen = 32;

        struct Entry {
            char      name[96];
            uintptr_t guardVA;
            uint8_t   guard[kMaxPatchLen];
            size_t    guardLen;
            uintptr_t writeVA;
            uint8_t   replacement[kMaxPatchLen];
            size_t    replLen;
            bool      isData;
            bool      dataIsFloat;
            float     dataExpected;
        };

        struct JournalEntry {
            uintptr_t va;
            uint8_t   original[kMaxPatchLen];
            size_t    len;
            char      name[96];
        };

        std::vector<Entry>        gPending;
        char                      gGroupName[96] = "";
        std::vector<JournalEntry> gJournal;
        int gApplied = 0, gSkippedGroups = 0, gFailed = 0;

        void SetName(char* dst, size_t cap, const char* src) {
            std::strncpy(dst, src ? src : "?", cap - 1);
            dst[cap - 1] = '\0';
        }

        Entry* NewEntry(const char* name) {
            gPending.emplace_back();
            Entry& e = gPending.back();
            std::memset(&e, 0, sizeof(e));
            SetName(e.name, sizeof(e.name), name);
            return &e;
        }

        bool ValidateEntry(const Entry& e) {
            if (e.isData) {
                if (e.dataIsFloat) {
                    float cur = 0.0f;
                    if (!Memory::ReadFloat(e.guardVA, &cur)) {
                        Log::Warn("%s: data guard unreadable at 0x%08lX.", e.name,
                                  static_cast<unsigned long>(e.guardVA));
                        return false;
                    }
                    float repl = 0.0f;
                    std::memcpy(&repl, e.replacement, sizeof(float));
                    if (std::fabs(cur - e.dataExpected) > 0.001f && std::fabs(cur - repl) > 0.001f) {
                        Log::Warn("%s: data guard failed at 0x%08lX (current %.6f, expected vanilla %.6f).",
                                  e.name, static_cast<unsigned long>(e.guardVA), cur, e.dataExpected);
                        return false;
                    }
                    return true;
                }
                uint8_t cur = 0;
                if (!Memory::ReadU8(e.guardVA, &cur)) {
                    Log::Warn("%s: data guard unreadable at 0x%08lX.", e.name,
                              static_cast<unsigned long>(e.guardVA));
                    return false;
                }
                if (cur > 1) {
                    Log::Warn("%s: data bool at 0x%08lX has unexpected value %u.",
                              e.name, static_cast<unsigned long>(e.guardVA), cur);
                    return false;
                }
                return true;
            }

            if (Memory::CheckBytes(e.guardVA, e.guard, e.guardLen)) return true;

            uint8_t actual[kMaxPatchLen]{};
            char expHex[kMaxPatchLen * 2 + 1]{}, actHex[kMaxPatchLen * 2 + 1]{};
            Log::HexString(e.guard, static_cast<unsigned>(e.guardLen), expHex, sizeof(expHex));
            if (Memory::ReadBytes(e.guardVA, actual, e.guardLen))
                Log::HexString(actual, static_cast<unsigned>(e.guardLen), actHex, sizeof(actHex));
            else
                SetName(actHex, sizeof(actHex), "<unreadable>");
            Log::Warn("%s: byte guard failed at 0x%08lX. expected=%s actual=%s",
                      e.name, static_cast<unsigned long>(e.guardVA), expHex, actHex);
            return false;
        }

        bool ApplyEntry(const Entry& e, JournalEntry& j) {
            j.va = e.writeVA;
            j.len = e.replLen;
            SetName(j.name, sizeof(j.name), e.name);
            if (!Memory::ReadBytes(e.writeVA, j.original, e.replLen)) return false;
            if (!Memory::WriteBytes(e.writeVA, e.replacement, e.replLen)) return false;
            return true;
        }

    } // namespace

    void Begin(const char* groupName) {
        gPending.clear();
        SetName(gGroupName, sizeof(gGroupName), groupName);
    }

    void AddFloatOperand(const char* name, uintptr_t insnVA,
                         const uint8_t* guard6, const float* replacement) {
        Entry* e = NewEntry(name);
        e->guardVA = insnVA;
        std::memcpy(e->guard, guard6, 6);
        e->guardLen = 6;
        e->writeVA = insnVA + 2;
        const uint32_t addr = reinterpret_cast<uint32_t>(replacement);
        std::memcpy(e->replacement, &addr, 4);
        e->replLen = 4;
    }

    void AddBytes(const char* name, uintptr_t va,
                  const uint8_t* guard, size_t guardLen,
                  const uint8_t* replacement, size_t replLen) {
        if (guardLen > kMaxPatchLen || replLen > guardLen) {
            Log::Error("%s: invalid patch lengths (guard %u repl %u).", name,
                       static_cast<unsigned>(guardLen), static_cast<unsigned>(replLen));
            gFailed++;
            return;
        }
        Entry* e = NewEntry(name);
        e->guardVA = va;
        std::memcpy(e->guard, guard, guardLen);
        e->guardLen = guardLen;
        e->writeVA = va;
        std::memcpy(e->replacement, replacement, replLen);
        e->replLen = replLen;
    }

    void AddImm8(const char* name, uintptr_t insnVA,
                 const uint8_t* guard, size_t guardLen,
                 uintptr_t immVA, uint8_t value) {
        Entry* e = NewEntry(name);
        e->guardVA = insnVA;
        std::memcpy(e->guard, guard, guardLen);
        e->guardLen = guardLen;
        e->writeVA = immVA;
        e->replacement[0] = value;
        e->replLen = 1;
    }

    void AddImm32(const char* name, uintptr_t insnVA,
                  const uint8_t* guard, size_t guardLen,
                  uintptr_t immVA, uint32_t value) {
        Entry* e = NewEntry(name);
        e->guardVA = insnVA;
        std::memcpy(e->guard, guard, guardLen);
        e->guardLen = guardLen;
        e->writeVA = immVA;
        std::memcpy(e->replacement, &value, 4);
        e->replLen = 4;
    }

    void AddDataFloat(const char* name, uintptr_t va, float expected, float value) {
        Entry* e = NewEntry(name);
        e->guardVA = va;
        e->writeVA = va;
        e->isData = true;
        e->dataIsFloat = true;
        e->dataExpected = expected;
        std::memcpy(e->replacement, &value, 4);
        e->replLen = 4;
    }

    void AddDataBool(const char* name, uintptr_t va, uint8_t value) {
        Entry* e = NewEntry(name);
        e->guardVA = va;
        e->writeVA = va;
        e->isData = true;
        e->dataIsFloat = false;
        e->replacement[0] = value;
        e->replLen = 1;
    }

    bool Commit() {
        if (gPending.empty()) return true;

        for (const Entry& e : gPending) {
            if (!ValidateEntry(e)) {
                Log::Warn("Group '%s' skipped entirely (%u patches) - guard failure above.",
                          gGroupName, static_cast<unsigned>(gPending.size()));
                gSkippedGroups++;
                gPending.clear();
                return false;
            }
        }

        const size_t journalStart = gJournal.size();
        for (const Entry& e : gPending) {
            JournalEntry j{};
            if (!ApplyEntry(e, j)) {
                Log::Error("Group '%s': write failed for '%s' at 0x%08lX - rolling back group.",
                           gGroupName, e.name, static_cast<unsigned long>(e.writeVA));
                for (size_t i = gJournal.size(); i > journalStart; --i) {
                    const JournalEntry& u = gJournal[i - 1];
                    Memory::WriteBytes(u.va, u.original, u.len);
                }
                gJournal.resize(journalStart);
                gFailed++;
                gPending.clear();
                return false;
            }
            gJournal.push_back(j);
        }

        gApplied += static_cast<int>(gPending.size());
        Log::Info("Group '%s': applied %u patch(es).", gGroupName,
                  static_cast<unsigned>(gPending.size()));
        gPending.clear();
        return true;
    }

    int AppliedCount()      { return gApplied; }
    int SkippedGroupCount() { return gSkippedGroups; }
    int FailedCount()       { return gFailed; }

    void RestoreAll() {
        for (size_t i = gJournal.size(); i > 0; --i) {
            const JournalEntry& u = gJournal[i - 1];
            if (!Memory::WriteBytes(u.va, u.original, u.len))
                Log::Warn("Restore failed for '%s' at 0x%08lX.", u.name,
                          static_cast<unsigned long>(u.va));
        }
        Log::Info("Restored %u journaled write(s).", static_cast<unsigned>(gJournal.size()));
        gJournal.clear();
    }

} // namespace Patch
