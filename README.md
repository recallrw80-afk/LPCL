# LPCL — Linux Plain Craft Launcher

**English** | [简体中文](README.zh-CN.md)

A cross-platform Minecraft launcher written in C++20 / Qt 6, centered on the lightweight command-line frontend `lpcl` (RSS ~4MB) — suitable for desktops, servers, and headless environments.

> This is an independently developed open-source project with no affiliation to or endorsement from [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2). Its public implementation ideas were referenced during development, but no code or assets were taken. It is likewise unaffiliated with Mojang / Microsoft (see the disclaimer at the end).

## Features

- **CLI first**: modpack import, instance management, version download, and game launch all work in a terminal; interactive pickers and wizards on TTY (create-vite style)
- **Modpack import**: CurseForge / Modrinth / MultiMC / HMCL / PCL launcher packs / compressed `.minecraft` / plain mod packs; automatic download of game files, modloaders (Forge / NeoForge / Fabric), and mods
- **Isolated instances**: each instance gets its own directory; `.incomplete` marker + full rollback on failure — no half-installed instances
- **Vanilla download**: install any MC version with one command (latest release by default); re-running verifies and repairs
- **Java management**: detects system Java, picks per MC version compatibility matrix, auto-downloads JRE from Adoptium when missing
- **Player profiles**: multiple offline player profiles (name / avatar / skin type) with interactive add/edit/remove/select; authlib-injector external login (e.g. LittleSkin) with encrypted, persisted sessions auto-refreshed online at launch
- **Launch stability**: automatic memory sizing (50% of available RAM, capped at 16G), GC tiers, auto-fix for fcitx/ibus XIM crashes (GLFW 3.4 replacement), full launch logs on disk
- **Local server hosting**: install and run vanilla/Forge/Fabric/NeoForge servers with two commands (`server-install`/`server-start`), console attached, headless-friendly
- **QML GUI** (test version): shares the same SDK (liblpclcore) with the CLI

## Install

### One-liner (recommended)

Official prebuilt package (CI-built, embedded CurseForge key — the full experience), for x86_64 / aarch64:

```bash
curl -fsSL https://github.com/recallrw80-afk/LPCL/releases/latest/download/install.sh | bash
```

Or grab the tarball from [Releases](https://github.com/recallrw80-afk/LPCL/releases/latest), put it next to `install.sh`, and run `bash install.sh lpcl-linux-<arch>.tar.gz`.

### Build from source

For hacking/custom builds. Note: locally built packages do not embed a CF key (CurseForge falls back to the MCIM mirror); use the official package for the full experience.

```bash
git clone https://github.com/recallrw80-afk/LPCL.git
cd LPCL
make install        # build Release → zero-dependency package → install to ~/.local
```

Requirements: Qt 6.11+ (Core/Network), CMake 3.16+, Ninja, a C++20 compiler, nlohmann-json 3.11+, ZLIB. Qt prefix defaults to `$HOME/Qt/6.11.1/gcc_64`; override with `make install QT_PREFIX=/path/to/Qt/6.x/gcc_64`.

To install a package built on another machine: `bash cli/install.sh lpcl-linux-<arch>.tar.gz`.

### Update & uninstall

```bash
lpcl update         # check GitHub Releases and update in place (install.sh-installed copies only)
lpcl uninstall      # uninstall (clears the game folder)
lpcl uninstall -r   # uninstall but keep game folder contents
```

### Common build targets

```bash
make cli            # build lpcl + liblpclcore.so (Debug)
make package-tar    # zero-dependency tarball at cli/dist/lpcl-linux-<arch>.tar.gz
make run            # build & launch the QML GUI (test version, binary name lpcl-gui)
```

## Quick start

```bash
# Import a modpack (game folder defaults to ./mc/ next to the binary; change with set-folder)
lpcl inpack ~/Downloads/some-modpack.zip

# List instances and pick one interactively to launch
lpcl launch

# Or play vanilla
lpcl mc-install        # latest release
lpcl launch 1.20.1
```

See [cli/README.md](cli/README.md) for the full command reference, or run `lpcl help`.

## Docs

- [cli/README.md](cli/README.md) — CLI command reference, directory layout, FAQ
- [CONTRIBUTING.md](CONTRIBUTING.md) — development conventions and contribution guide

## Contributing

Issues and PRs are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md) before submitting.

## Disclaimer

- This project is not an official Minecraft product and is not associated with, approved by, or endorsed by Mojang Studios / Microsoft. "Minecraft" is a trademark of Mojang Synergies AB.
- This project has no affiliation with or authorization from the official Plain Craft Launcher (PCL); "LPCL" merely denotes a similar launcher targeting Linux.
- This software is provided "as is"; the authors accept no liability for damages arising from its use (see sections 15–16 of the license).

## Credits

- [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) — implementation ideas reference
- [PCL Community Edition](https://github.com/PCL-Community/PCL2-CE) — selected solutions reference (e.g. the MCIM mirror)
- Qt, nlohmann-json, zlib, Adoptium, BMCLAPI/MCIM mirrors, and other upstream projects

## License

[GNU General Public License v3.0](LICENSE)

Copyright (C) 2026 LPCL authors. This program is free software: you may redistribute and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 3. Derivative works must be open-sourced under the same license.
