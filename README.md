# OpenNeoUA

OpenNeoUA is an independent, open-source and non-commercial evolution of the `UA_source`/OpenUA engine for **Urban Assault**. It modernizes real engine limitations while preserving vanilla data, levels, scripts, saves and the original game feeling. A legitimate copy of the original game data is still required to play.

The project is derived from the upstream [Marisa-Chan/UA_source](https://github.com/Marisa-Chan/UA_source) lineage and keeps that provenance visible. Microsoft, TerraTools and the other rights holders are not affiliated with or endorsing this project. Existing credits and notices remain applicable.

**License:** GPLv2

# OpenNeoUA Parameter Guide

OpenNeoUA extends Urban Assault with a growing collection of optional, data-driven parameters that can be used to customize gameplay, AI, vehicles, weapons, visual effects, user interfaces, level behavior and many other engine systems.

These extensions are one of the main ways OpenNeoUA can go beyond the original game while remaining compatible with existing Urban Assault data. Most OpenNeoUA-specific features are optional and are designed to preserve vanilla behavior when their parameters are not used.

The repository includes:

**`OpenNeoUA_Parameter_Guide.ini`**

This file serves as the public reference for OpenNeoUA-specific parameters. It contains example values, descriptions, supported ranges, fallback behavior, visual asset priorities, configuration notes and explanations of how many of the extended systems interact with the original engine.

The guide is intended both for experienced Urban Assault modders and for users who are discovering OpenNeoUA's extended scripting capabilities for the first time.

OpenNeoUA is under continuous development, and new parameters and systems are added regularly. The Parameter Guide will be updated as soon as reasonably possible after new functionality is introduced. Because development can move faster than documentation, some of the newest parameters or recently changed behavior may not yet be present in the guide.

For the latest implementation details, the current OpenNeoUA source code and runtime behavior remain authoritative.

# Building and Installing OpenNeoUA on Modern Windows (64-bit MSYS2)

OpenNeoUA is currently distributed as source code. On Windows, the executable must first be built with the 64-bit MinGW environment provided by MSYS2.

## 1. Install and update MSYS2

1. Download and install MSYS2:

   https://www.msys2.org/

2. Open the standard **MSYS2 MSYS** terminal.

3. Update MSYS2:

   ```bash
   pacman -Syu
   ```

   If MSYS2 asks you to close the terminal, close it, reopen **MSYS2 MSYS**, and continue the update before proceeding.

4. Install the required 64-bit development packages:

   ```bash
   pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_image mingw-w64-x86_64-SDL2_ttf mingw-w64-x86_64-SDL2_net mingw-w64-x86_64-openal mingw-w64-x86_64-libvorbis mingw-w64-x86_64-ffmpeg mingw-w64-x86_64-lua
   ```

## 2. Assemble the original game data and OpenNeoUA

5. Obtain a legitimate, clean and unmodified installation of **Urban Assault**. Use a separate folder for OpenNeoUA so your original installation remains untouched. This repository does not supply the proprietary original game scripts or assets.

6. Create a destination folder, for example:

   ```text
   C:\Games\Urban Assault\Data\
   ```

7. Copy the **contents** of the original `Microsoft Urban Assault\DATA\` folder directly into that destination `Data` folder. Do not copy the `DATA` folder itself inside it: there must be no `Data\Data\` nesting.

8. Copy the original game's other folders and configuration into the same destination `Data` folder: `LEVELS`, `LOCALE`, `ENV`, `SAVE`, `HELP`, `MUSIC` and `NUCLEUS.INI`, where supplied by your installation. Keep their contents and names. The engine supports this Data-first layout and also retains compatibility with the original locations beside the executable.

   For example, the original `Microsoft Urban Assault\LEVELS\` becomes `Urban Assault\Data\LEVELS\`, and the original `Microsoft Urban Assault\NUCLEUS.INI` becomes `Urban Assault\Data\NUCLEUS.INI`.

   If the original installation supplies `DUNGEON.TTF`, copy that font into `Data\Fonts\` to make the original menu font available to the engine.

9. Download the OpenNeoUA source ZIP from [the OpenNeoUA repository](https://github.com/TeuZzZ-17/OpenNeoUA). Extract it and copy the **contents** of the extracted project folder into `Urban Assault\Data\`. Merge folders and replace matching files with the repository version. Do not place the enclosing `OpenNeoUA-main` folder inside `Data`.

   Copy the original data first and the repository contents second. Both sets of files are required. The repository adds engine files and configuration; it does not replace the original game data.

10. Before building, check these example paths:

    ```text
    Urban Assault\Data\src\CMakeLists.txt
    Urban Assault\Data\World.ini
    Urban Assault\Data\Scripts\Startup.cfg
    Urban Assault\Data\Scripts\STARTUP.SCR
    Urban Assault\Data\Scripts\FEINDE.SCR
    Urban Assault\Data\Scripts\ROBOS.SCR
    Urban Assault\Data\LEVELS\
    Urban Assault\Data\LOCALE\
    ```

    `Startup.cfg` loads the original `startup.scr` manifest first, preserving its original include order, then loads OpenNeoUA/mod definitions when available. The original manifest supplies vanilla vehicles, weapons, buildings, host stations, effects and sounds. Do not remove the original `.scr` files.

    `Weapons.cfg`, `Buildings.cfg`, `Effects.cfg` and `Vehicles.cfg` are not supplied by this repository. They are optional additions supplied separately by a mod installation. Missing prototype includes are logged and skipped; errors inside existing scripts still fail loading. Skipping a missing file cannot replace required original definitions or assets.

## 3. Build OpenNeoUA

11. Close the standard MSYS terminal and open **MSYS2 MinGW x64 / MinGW 64-bit** (`C:\msys64\mingw64.exe`). Verify that the prompt contains `MINGW64`.

12. In that terminal, go to your assembled `Data` folder:

    ```bash
    cd "/c/Games/Urban Assault/Data"
    ```

    Adjust the path to your installation. Keep the quotes if the path contains spaces.

13. Configure and build:

    ```bash
    cmake -B build -S src
    cmake --build build -j12
    ```

    `-j12` uses up to 12 parallel build jobs; adjust it for your CPU. These commands run inside `Data`, so a successful build produces:

    ```text
    Urban Assault\Data\build\OpenNeoUA.exe
    ```

14. Copy that executable into the **main Urban Assault folder**, one level above `Data`:

    ```text
    C:\Games\Urban Assault\OpenNeoUA.exe
    ```

## 4. Install the Windows runtime DLLs

15. Open `C:\msys64\mingw64\bin` and copy all `.dll` files into the main Urban Assault folder beside `OpenNeoUA.exe`.

    Copying all DLLs is the simple installation method and includes direct and indirect dependencies. Keep the runtime executable and its DLLs together in the main folder.

    ```text
    Urban Assault\
    ├── OpenNeoUA.exe
    ├── [runtime DLL files]
    └── Data\
        ├── [original Urban Assault data and support folders]
        └── [current OpenNeoUA repository contents, merged here]
    ```

    The last two lines describe merged contents, not two extra folders to create. For example, original and repository scripts share `Data\Scripts\`.

## 5. Run and verify OpenNeoUA

16. Launch `OpenNeoUA.exe` from Windows Explorer. If you use a shortcut, set its **Start in** folder to the main Urban Assault folder containing that executable. From a terminal, change to that folder before launching.

17. Create/select a profile and start a campaign mission or tutorial. Reaching the menu alone does not verify that the original prototype scripts and level assets loaded correctly.

    On first loading an original retail profile, OpenNeoUA applies and saves its current Options-page and input defaults automatically. Subsequent launches preserve your changes. Existing engine profiles with saved extended video settings are preserved. Advanced graphics start from the shipped OpenNeoUA graphics profile, with `OpenNeoUA.ini` providing the higher-priority settings.

    Once the game starts from Explorer with DLLs beside it, the installation can run outside MSYS2. Launch `OpenNeoUA.exe` to use this engine.

## Development status and troubleshooting

OpenNeoUA is under continuous development. Bugs and crashes may occur, especially when original data is missing, mixed with incompatible mod files, or copied into the wrong folders. A successful build does not guarantee a complete game installation.

If startup or a level fails, read:

```text
Urban Assault\Data\Env\ypa_log.txt
```

In a legacy layout, the log may instead be in `Urban Assault\Env\ypa_log.txt`. Keep a copy of the log before restarting: it is rewritten when the engine starts. When reporting a problem, include the log, engine version, failing level and your folder layout.

- `missing include ...; skipped` identifies a file absent from a prototype manifest. Missing optional mod `.cfg` files can be normal on a vanilla installation. Missing original `.scr` files mean the original data needs to be restored.
- `PARSE ERROR` or `include ... failed` identifies an existing script that could not be loaded correctly. Check the reported file and line.
- `no host robo for squad` means a squad's host station was not created. Check earlier errors, `Data\Scripts\Startup.cfg`, the original `STARTUP.SCR` and its included files, and the level's required models. The warning alone does not establish the cause of a crash.
- Missing files under `Levels`, `Locale` or the SET folders indicate incomplete or incorrectly placed original data. Recheck steps 7–10 and avoid `Data\Data` or `Data\OpenNeoUA-main` nesting.

## Installation layout and future versions

The installation rule is:

```text
OpenNeoUA.exe + runtime DLLs                      → Urban Assault\
Original DATA contents + original support folders
    + current OpenNeoUA repository contents      → Urban Assault\Data\
```

New resources may be added as development continues. When updating, merge the new repository contents into `Data`, rebuild, and copy the newly built executable to the main folder. Preserve your original data and back up custom configuration/mod files before replacing matching files.

### Source files

Keeping:

```text
Data\src\
Data\svg\
```

is supported and keeps the OpenNeoUA source conveniently available alongside the game.

The generated `build` folder is not required to run OpenNeoUA after `OpenNeoUA.exe` has been copied to the main game folder and may be deleted if desired.

### Recommended SET organization

OpenNeoUA supports the traditional Urban Assault SET layout, but the cleaner organized layout is strongly recommended.

Instead of:

```text
Data\SET1\
Data\SET2\
Data\SET3\
...
```

you can organize the SET directories as:

```text
Data\Sets\Set1\
Data\Sets\Set2\
Data\Sets\Set3\
...
```

OpenNeoUA supports the organized `Data\Sets\Set_` layout while retaining compatibility with the legacy SET locations.

## Intellectual Property and Original Game Data

OpenNeoUA is an independent, community-developed, non-commercial open-source project based on the publicly available `UA_source` / OpenUA code lineage. It is not affiliated with, sponsored by, endorsed by, or officially approved by Microsoft, TerraTools, or any other current or former rights holder of Urban Assault.

Urban Assault, including its name, trademarks, original game data, artwork, interface artwork, audio, music, models, textures, levels, cinematics, and other original game content, remains the property of its respective rights holders.

OpenNeoUA is intended to provide an engine implementation and original project-specific additions. The repository is not intended to distribute the proprietary data files of the original Urban Assault game. A user must supply a lawfully obtained copy of the original game data where such data is required for operation.

The GNU General Public License version 2 applies only to source code and other material in this repository that is actually distributed under that license. The GPL does not grant any rights in third-party trademarks, copyrighted game assets, or other material owned by third parties.

Files contributed specifically to OpenNeoUA may have their own authorship or licensing status where stated. Inclusion of a compatibility reference, filename, format name, game name, screenshot, description, or technical identifier does not imply ownership of the corresponding third-party intellectual property.

No ownership is claimed over Urban Assault or over proprietary material belonging to Microsoft, TerraTools, or any other rights holder.

If you are a rights holder and believe that material has been included in this repository in error, please contact the repository owner so the material can be reviewed and, where appropriate, removed.
