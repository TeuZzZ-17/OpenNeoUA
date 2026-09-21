# OpenNeoUA

OpenNeoUA is an independent, open-source and non-commercial evolution of the `UA_source`/OpenUA engine for **Urban Assault**. It modernizes real engine limitations while preserving vanilla data, levels, scripts, saves and the original game feeling. A legitimate copy of the original game data is still required to play.

he project is derived from the upstream [Marisa-Chan/UA_source](https://github.com/Marisa-Chan/UA_source) lineage and keeps that provenance visible. Microsoft, TerraTools and the other rights holders are not affiliated with or endorsing this project. Existing credits and notices remain applicable.

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

## 2. Download and build OpenNeoUA

5. Download the OpenNeoUA source code ZIP from:

   https://github.com/TeuZzZ-17/OpenNeoUA

6. Extract the project folder. For example:

   ```text
   C:\Users\YourName\Desktop\OpenNeoUA
   ```

7. **Important: do not compile from the standard MSYS terminal.**

   Close the standard MSYS terminal and open **MSYS2 MinGW x64 / MinGW 64-bit**.

   You can also start it directly with:

   ```text
   C:\msys64\mingw64.exe
   ```

   Before continuing, verify that the terminal prompt contains:

   ```text
   MINGW64
   ```

   and not:

   ```text
   MSYS
   ```

8. In the **MINGW64** terminal, go to the extracted OpenNeoUA folder. Example:

   ```bash
   cd /c/Users/YourName/Desktop/OpenNeoUA
   ```

9. Configure the project:

   ```bash
   cmake -B build -S src
   ```

10. Compile OpenNeoUA:

   ```bash
   cmake --build build -j12
   ```

   `-j12` uses up to 12 parallel build jobs. You may use a different value if appropriate for your CPU.

11. If compilation succeeds, the executable will be created here:

   ```text
   build\OpenNeoUA.exe
   ```

## 3. Prepare the Urban Assault installation

12. Obtain a legitimate, clean and unmodified installation of the original **Urban Assault**.

   Using a separate copy of the original game for OpenNeoUA is strongly recommended so that the original installation remains untouched.

13. Open the original game's `Data` folder.

   Example:

   ```text
   C:\Games\Urban Assault\Data\
   ```

14. Copy the **complete contents of the extracted OpenNeoUA project folder** into the original Urban Assault `Data` folder.

   In other words:

   ```text
   OpenNeoUA project contents
           ↓
   Urban Assault\Data\
   ```

   Allow Windows to merge folders and replace OpenNeoUA files when the same destination file already exists.

   Copy the repository contents as they are provided by the current OpenNeoUA version. The exact set of folders and files may change over time as the engine evolves and new systems or resources are added.

   The original Urban Assault data must remain present. OpenNeoUA does **not** redistribute the complete original game data.

15. Copy the newly compiled executable:

   ```text
   OpenNeoUA\build\OpenNeoUA.exe
   ```

   into the **main Urban Assault folder**, one level above `Data`.

   Example:

   ```text
   C:\Games\Urban Assault\OpenNeoUA.exe
   ```

   Do **not** place the runtime executable inside `Data`.

## 4. Install the Windows runtime DLLs

16. Open:

   ```text
   C:\msys64\mingw64\bin
   ```

17. Copy **all `.dll` files** from that folder into the same main Urban Assault folder that contains `OpenNeoUA.exe`.

   Do not copy the DLLs into `Data`.

   The installation should follow this general structure:

   ```text
   Urban Assault\
   ├── OpenNeoUA.exe
   ├── [runtime DLL files]
   │
   └── Data\
       ├── [original Urban Assault data]
       └── [current OpenNeoUA repository contents]
   ```

   This layout is intentionally shown only at a high level. The exact files and subfolders used by OpenNeoUA may change over time as development continues. Always use the structure provided by the current repository version rather than relying on an old fixed folder list.

   Copying all DLL files is intentionally the simple installation method. It avoids forcing users to resolve the executable's direct and indirect runtime dependencies one file at a time.

## 5. Run OpenNeoUA

18. Launch:

   ```text
   OpenNeoUA.exe
   ```

   from Windows Explorer.

   Do not launch the original Urban Assault executable if you want to run the OpenNeoUA engine.

19. Once `OpenNeoUA.exe` starts correctly from Windows Explorer, the installation is portable outside the MSYS2 terminal environment, provided the required DLLs remain next to the executable.

## Installation layout and future versions

OpenNeoUA is under active development. New folders, resources or runtime files may be added, renamed or reorganized in future versions.

For this reason, the folder examples in this README are illustrative rather than a permanent specification of every file that OpenNeoUA will ever use.

The important rule is:

```text
OpenNeoUA.exe + runtime DLLs  → main Urban Assault folder
OpenNeoUA repository contents → Urban Assault\Data\
```

When installing a newer version, always follow the structure shipped by that version of the repository and merge its contents into `Data` unless the current documentation explicitly says otherwise.

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
