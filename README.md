# LPCL — Linux Plain Craft Launcher

Originally, there was a PCL launcher, but there was no Linux version or CLI version, so I needed to develop an open-source LPCL launcher myself that supports CLI to quickly boot Minecraft from the server.

LPCL is a cross-platform Minecraft launcher written in C++20 with Qt 6.11. It provides both a QML GUI and a lightweight CLI frontend (`lpcl-cli`, RSS ~4MB) suitable for server and headless environments.

## Features

- **CLI-first**: Import modpacks, list instances, and launch Minecraft entirely from the command line
- **Modpack import**: Supports CurseForge, Modrinth, MultiMC, HMCL, MCBBS, and PCL launcher packs
- **Full download pipeline**: Downloads Minecraft versions, libraries, assets, native libraries, and modloaders (Forge / NeoForge / Fabric)
- **Mod downloading**: Downloads mods from Modrinth (direct URLs) and CurseForge (API)
- **Instance isolation**: Each modpack lives in its own directory, no cross-contamination
- **Offline auth**: Mojang-standard offline UUID generation
- **BMCLAPI mirror**: Faster downloads for users in China

## Quick Start

### Prerequisites

- Qt 6.11+ (Core, Network)
- CMake 3.16+
- C++20 compiler (GCC 12+ / Clang 16+)
- nlohmann-json 3.11+
- ZLIB

### Build

```bash
cd LPCL

# CLI (headless, RSS ~4MB)
make cli

# CLI with custom Qt path
QT_PREFIX=/path/to/Qt/6.11.1/gcc_64 make cli

# QML GUI App
make run
```

### CLI Usage

```bash
# Set game directory
lpcl-cli set-folder "/home/user/.minecraft"

# Import a modpack
lpcl-cli inpack "整合包.zip"
lpcl-cli inpack "modpack.zip" --r "My Instance"

# List installed instances
lpcl-cli list

# Launch an instance
lpcl-cli launch "My Instance"

# Remove an instance
lpcl-cli rm "My Instance"
lpcl-cli rm '*'   # remove all

# List available Java runtimes
lpcl-cli list-javas
```

### Import Pipeline

```
inpack zip → detect type → extract to tmp/ → download MC → download natives
  → install modloader → download mods → move to game directory → write Setup.ini
```

Incomplete imports never appear in the game directory — everything stays in `tmp/` until fully downloaded.

## Architecture

```
LPCL/
├── cli/                     # CLI frontend (lpcl-cli)
├── sdk/                     # Shared library (liblpclcore)
│   ├── include/             # Public headers
│   │   ├── lpcl.h           #   SDK entry point
│   │   ├── core/            #   Settings, VersionManager, Launcher, Installer
│   │   ├── auth/            #   OfflineAuth, MsAuth, AuthlibAuth
│   │   ├── download/        #   DownloadManager, AssetDownloader, ModPlatform
│   │   └── modpack.h        #   Modpack detection & installation
│   └── src/                 # Implementation
├── gui/                     # QML GUI App
│   ├── ui/                  #   QML pages & components
│   └── assets/              #   SVG icons (Lucide)
└── PCL/                     # Original VB.NET reference source
```

## Supported Modpack Formats

| Format | Detection | MC Download | Modloader | Mods |
|--------|-----------|-------------|-----------|------|
| CurseForge | `manifest.json` | ✅ | ✅ Forge/NeoForge/Fabric | ✅ via API |
| Modrinth | `modrinth.index.json` | ✅ | ✅ | ✅ direct URL |
| MultiMC | `mmc-pack.json` | ✅ | ✅ | — |
| HMCL | `modpack.json` | ✅ | — | — |
| MCBBS | `mcbbs.packmeta` | ✅ | ✅ | — |
| PCL Launcher Pack | `PCL/Setup.ini` + `PCL/LatestLaunch.bat` | ✅ | — | — |
| Compressed .minecraft | `versions/X/X.json` | ✅ | — | — |

## License

MIT
