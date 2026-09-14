#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "ExeIdentity.h"
#include "Addresses.h"
#include "Log.h"

namespace ExeIdentity {

    bool IsSupported() {
        const HMODULE exe = GetModuleHandleW(nullptr);
        if (!exe) return false;

        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(reinterpret_cast<const uint8_t*>(exe) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) return false;

        const uintptr_t base  = reinterpret_cast<uintptr_t>(exe);
        const uint32_t  stamp = nt->FileHeader.TimeDateStamp;
        const uint32_t  size  = nt->OptionalHeader.SizeOfImage;

        if (base == Addr::kImageBase && nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386
            && stamp == Addr::kTimeDateStamp && size == Addr::kSizeOfImage) {
            Log::Info("speed.exe v1.3 confirmed.");
            return true;
        }

        Log::Error("This speed.exe is not the English PC v1.3 build (timestamp 0x%08lX, image size 0x%08lX). "
                   "HelicopterOptions changed nothing.",
                   static_cast<unsigned long>(stamp), static_cast<unsigned long>(size));
        return false;
    }

}
