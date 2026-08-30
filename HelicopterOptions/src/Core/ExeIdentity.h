// ExeIdentity.h - strict executable identity validation.
// Supported: NFSMW 1.3 speed.exe, TimeDateStamp 0x438E4C8C, base 0x00400000,
// SizeOfImage 0x00678E4E.
#pragma once

namespace ExeIdentity {

    enum Result {
        kSupported,
        kUnsupported,
        kNotAnExe
    };

    Result Validate();

} // namespace ExeIdentity
