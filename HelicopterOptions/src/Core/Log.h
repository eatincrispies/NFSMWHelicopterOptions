#pragma once

namespace Log {

    void Open(void* module);
    void Close();
    void CheckRotate();

    void Info(const char* format, ...);
    void Warn(const char* format, ...);
    void Error(const char* format, ...);

    void Hex(const unsigned char* bytes, unsigned length, char* out, unsigned outLength);

}
