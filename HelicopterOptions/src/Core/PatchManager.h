#pragma once
#include <cstdint>
#include "Addresses.h"

namespace Patch {

    void Begin(const char* group);
    void RedirectFloat(const char* name, const Addr::FloatOperand& site, const float* value);
    void PushFloat(const char* name, const Addr::PushFloat& site, float value);
    void DataFloat(const char* name, uintptr_t va, float vanilla, float value);
    bool Commit();

    int  SkippedGroups();
    int  FailedGroups();
    void RestoreAll();

}
