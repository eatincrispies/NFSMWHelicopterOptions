#include <cmath>
#include <cstring>
#include <vector>
#include "PatchManager.h"
#include "Log.h"
#include "Memory.h"

namespace Patch {

    namespace {

        struct Entry {
            const char* name;
            uintptr_t   guardVa;
            uint8_t     guard[6];
            size_t      guardLength;
            uintptr_t   writeVa;
            uint8_t     bytes[4];
            bool        data;
            float       vanilla;
        };

        struct Original {
            const char* name;
            uintptr_t   va;
            uint8_t     bytes[4];
        };

        std::vector<Entry>    gPending;
        std::vector<Original> gJournal;
        const char*           gGroup = "";
        int                   gSkipped = 0;
        int                   gFailed = 0;

        bool Check(const Entry& entry) {
            if (entry.data) {
                float current = 0.0f;
                float wanted = 0.0f;
                std::memcpy(&wanted, entry.bytes, sizeof(wanted));
                if (!Memory::Read(entry.guardVa, &current, sizeof(current))) {
                    Log::Warn("%s: %s at 0x%08lX could not be read.", gGroup, entry.name,
                              static_cast<unsigned long>(entry.guardVa));
                    return false;
                }
                if (std::fabs(current - entry.vanilla) <= 0.001f || std::fabs(current - wanted) <= 0.001f)
                    return true;
                Log::Warn("%s: %s at 0x%08lX holds %g instead of the game's %g.", gGroup, entry.name,
                          static_cast<unsigned long>(entry.guardVa), current, entry.vanilla);
                return false;
            }

            if (Memory::CheckBytes(entry.guardVa, entry.guard, entry.guardLength)) return true;

            uint8_t actual[6] = {};
            char expectedHex[16] = "";
            char actualHex[16] = "unreadable";
            Log::Hex(entry.guard, static_cast<unsigned>(entry.guardLength), expectedHex, sizeof(expectedHex));
            if (Memory::Read(entry.guardVa, actual, entry.guardLength))
                Log::Hex(actual, static_cast<unsigned>(entry.guardLength), actualHex, sizeof(actualHex));
            Log::Warn("%s: %s at 0x%08lX expected %s, found %s.", gGroup, entry.name,
                      static_cast<unsigned long>(entry.guardVa), expectedHex, actualHex);
            return false;
        }

    }

    void Begin(const char* group) {
        gPending.clear();
        gGroup = group;
    }

    void RedirectFloat(const char* name, const Addr::FloatOperand& site, const float* value) {
        Entry entry = {};
        entry.name = name;
        entry.guardVa = site.va;
        entry.guard[0] = site.opcode[0];
        entry.guard[1] = site.opcode[1];
        const uint32_t constant = static_cast<uint32_t>(site.constant);
        std::memcpy(entry.guard + 2, &constant, sizeof(constant));
        entry.guardLength = 6;
        entry.writeVa = site.va + 2;
        const uint32_t target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(value));
        std::memcpy(entry.bytes, &target, sizeof(target));
        gPending.push_back(entry);
    }

    void PushFloat(const char* name, const Addr::PushFloat& site, float value) {
        Entry entry = {};
        entry.name = name;
        entry.guardVa = site.va;
        entry.guard[0] = 0x68;
        std::memcpy(entry.guard + 1, &site.value, sizeof(site.value));
        entry.guardLength = 5;
        entry.writeVa = site.va + 1;
        std::memcpy(entry.bytes, &value, sizeof(value));
        gPending.push_back(entry);
    }

    void DataFloat(const char* name, uintptr_t va, float vanilla, float value) {
        Entry entry = {};
        entry.name = name;
        entry.guardVa = va;
        entry.writeVa = va;
        entry.data = true;
        entry.vanilla = vanilla;
        std::memcpy(entry.bytes, &value, sizeof(value));
        gPending.push_back(entry);
    }

    bool Commit() {
        for (const Entry& entry : gPending) {
            if (!Check(entry)) {
                Log::Warn("%s: skipped, because another mod has already changed this code.", gGroup);
                ++gSkipped;
                gPending.clear();
                return false;
            }
        }

        const size_t start = gJournal.size();
        for (const Entry& entry : gPending) {
            Original original = {};
            original.name = entry.name;
            original.va = entry.writeVa;
            if (!Memory::Read(entry.writeVa, original.bytes, sizeof(original.bytes))
                || !Memory::WriteCode(entry.writeVa, entry.bytes, sizeof(entry.bytes))) {
                Log::Error("%s: %s at 0x%08lX could not be written; the group was rolled back.", gGroup,
                           entry.name, static_cast<unsigned long>(entry.writeVa));
                while (gJournal.size() > start) {
                    const Original& undo = gJournal.back();
                    Memory::WriteCode(undo.va, undo.bytes, sizeof(undo.bytes));
                    gJournal.pop_back();
                }
                ++gFailed;
                gPending.clear();
                return false;
            }
            gJournal.push_back(original);
        }

        Log::Info("%s: %u patch(es) applied.", gGroup, static_cast<unsigned>(gPending.size()));
        gPending.clear();
        return true;
    }

    int SkippedGroups() {
        return gSkipped;
    }

    int FailedGroups() {
        return gFailed;
    }

    void RestoreAll() {
        while (!gJournal.empty()) {
            const Original& original = gJournal.back();
            if (!Memory::WriteCode(original.va, original.bytes, sizeof(original.bytes)))
                Log::Warn("%s at 0x%08lX could not be restored.", original.name,
                          static_cast<unsigned long>(original.va));
            gJournal.pop_back();
        }
    }

}
