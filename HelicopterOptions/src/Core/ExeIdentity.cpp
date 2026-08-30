#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include "ExeIdentity.h"
#include "Addresses.h"
#include "Log.h"

namespace ExeIdentity {

    Result Validate() {
        HMODULE exe = GetModuleHandleW(nullptr);
        if (!exe) { Log::Error("Cannot resolve host module."); return kNotAnExe; }

        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(exe);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) { Log::Error("Host has no DOS header."); return kNotAnExe; }
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(exe) + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE) { Log::Error("Host has no NT header."); return kNotAnExe; }

        const uintptr_t base      = reinterpret_cast<uintptr_t>(exe);
        const uint32_t  machine   = nt->FileHeader.Machine;
        const uint32_t  stamp     = nt->FileHeader.TimeDateStamp;
        const uint32_t  imageSize = nt->OptionalHeader.SizeOfImage;

        // Detailed identity values are logged only when diagnostics are on.
        Log::Verbose("Game module: base=0x%08lX machine=0x%04X timestamp=0x%08lX imageSize=0x%08lX "
                     "(supported: base=0x%08lX timestamp=0x%08lX imageSize=0x%08lX)",
                     static_cast<unsigned long>(base), machine,
                     static_cast<unsigned long>(stamp), static_cast<unsigned long>(imageSize),
                     static_cast<unsigned long>(Addr::kImageBase),
                     static_cast<unsigned long>(Addr::kTimeDateStamp),
                     static_cast<unsigned long>(Addr::kSizeOfImage));

        const bool ok = (base == Addr::kImageBase)
                      && (machine == IMAGE_FILE_MACHINE_I386)
                      && (stamp == Addr::kTimeDateStamp)
                      && (imageSize == Addr::kSizeOfImage);
        if (ok)
            Log::Info("Supported game version confirmed.");
        return ok ? kSupported : kUnsupported;
    }

} // namespace ExeIdentity
