// Ini.h - modular multi-file configuration loader (14 INIs, table-driven).
// Files: <moduleDir>\HelicopterOptions\Configuration\*.ini
#pragma once

namespace Ini {

    // Loads all configuration files into gCfg, runs Validation::Sanitize(),
    // syncs the live floats, and logs the startup summary. Returns false if
    // no configuration file could be loaded (built-in vanilla defaults apply).
    bool LoadConfig(void* moduleHandle);

} // namespace Ini
