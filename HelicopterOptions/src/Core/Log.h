// Log.h - file + debugger logging.
#pragma once

namespace Log {

    // Opens <moduleDir>\HelicopterOptions\Logs\HelicopterOptions.log
    // (directories created as needed). Safe to call before config load.
    // An existing log larger than 4 MB is rotated to .old at open.
    void Open(void* moduleHandle);
    void Close();

    // Session rotation: if the live log exceeds maxKB, rotate to .old and
    // reopen. Called periodically from the telemetry tick; cheap.
    void CheckRotate(int maxKB);

    void Info(const char* fmt, ...);
    void Verbose(const char* fmt, ...);   // only when verbose logging is on
    void Warn(const char* fmt, ...);
    void Error(const char* fmt, ...);

    void SetVerbose(bool on);
    void SetToFile(bool on);
    bool IsVerbose();

    // Format a byte range as hex ("D81DB80D8900"). bufLen must be >= len*2+1.
    void HexString(const unsigned char* bytes, unsigned len, char* buf, unsigned bufLen);

} // namespace Log
