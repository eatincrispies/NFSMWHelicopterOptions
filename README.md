# HelicopterOptions

Extra police helicopter tuning options for **Need for Speed: Most Wanted (2005)**.

This is the public source-code repo for `HelicopterOptions.asi`. The mod exposes helicopter behavior through `HelicopterOptions.ini`, including pursuit aggression, skid/strike behavior, lead/height tuning, acceleration, turn clamp, exit/fuel behavior, vision behavior, and experimental research toggles.

## Important

This repository is provided so the source code is available for review. It does **not** include any game executable, game assets, Ghidra dumps, or EA decompiled source.

The code here is original ASI patch/mod code. It applies byte-guarded patches to the existing game executable at runtime and reads values from `HelicopterOptions.ini`.

## Included files

```text
HelicopterOptions.slnx
HelicopterOptions/
  dllmain.cpp
  framework.h
  pch.cpp
  pch.h
  HelicopterOptions.vcxproj
  HelicopterOptions.vcxproj.filters
HelicopterOptions.ini
build_HelicopterOptions_win32_vs.bat
README.md
LICENSE.txt
.gitignore
```

## Building with the batch file

This mod is for the 32-bit PC version of NFSMW 2005, so it must be built as **Win32/x86**.

Run:

```bat
build_HelicopterOptions_win32_vs.bat
```

The batch file calls Visual Studio's x86 build environment and outputs:

```text
HelicopterOptions.asi
```

The important flags are:

```text
-arch=x86
/O2
/DNDEBUG
/LD
```

If Visual Studio is installed somewhere else, edit the path inside the `.bat` file.

## Building with Visual Studio

Open:

```text
HelicopterOptions.slnx
```

Use:

```text
Configuration: Release
Platform: Win32
```

Then build the project. If the output is a `.dll`, rename it to:

```text
HelicopterOptions.asi
```

## Installing

Place these files in the game's `scripts` folder:

```text
scripts/
  HelicopterOptions.asi
  HelicopterOptions.ini
```

The ASI and INI must have matching names. `HelicopterOptions.asi` looks for `HelicopterOptions.ini` next to itself.

## INI notes

Most public-safe tuning lives in:

```ini
[Patches::Core]
[Chopper::Skid]
[Chopper::LeadHeight]
[Chopper::Movement]
[Chopper::Exit]
[Chopper::HeliSheet]
[Chopper::Vision]
```

Experimental/risky sections should stay disabled by default for public release builds:

```ini
[Risky::Render]
[Risky::Extras]
[Risky::Dispatch]
```

Known limitation: multiple helicopters are not officially supported by the base game and may phase through each other.

## Credits

Created by eatincrispies / XeroAbsolute.

Developed with manual testing, reverse-engineering research, and LLM assistance.

## License

MIT License. See `LICENSE.txt`.
