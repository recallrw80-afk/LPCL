# lpcl — A Minecraft Launcher for Linux

**English** | [简体中文](README.zh-CN.md)

**LPCL (Linux Plain Craft Launcher)** is a cross-platform port of [Plain Craft Launcher (PCL)](https://github.com/Hex-Dragon/PCL2) — a Minecraft launcher written in C++/Qt. `lpcl` is its command-line frontend: no graphical interface needed, just a few MB of memory, with support for modpack import, multi-version management, and game launching.

## Features

- **Modpack import**: CurseForge / Modrinth / MCBBS / MultiMC / HMCL / launcher-shell packs / compressed `.minecraft` / plain mod packs — game files, modloaders (Forge/NeoForge/Fabric), and mods downloaded automatically
- **Multi-instance management**: isolated directory per instance; batch removal supported
- **Vanilla download**: one-command install of any MC version, with verify-and-repair
- **Java management**: detects system Java, picks per MC version automatically, downloads from Adoptium when missing
- **Player profiles**: multiple offline player profiles with skin types (slim/wide)
- **Resume & verify**: every download is SHA1-verified; re-running skips existing files
- **Chinese/English UI**: switch with `set-lang zh`

## Requirements

- Linux (x86_64 / aarch64)
- A Java runtime to play (don't worry — it's downloaded automatically on launch if missing)

## Install

### Build from source (recommended)

Clone the release source code, then compile and install on your machine (`~/.local/lib/lpcl/` + `~/.local/bin/lpcl`):

```bash
git clone https://github.com/recallrw80-afk/LPCL.git
cd LPCL
make install        # build Release → zero-dependency package → install
```

Requirements: Qt 6.11+ (Core/Network), CMake 3.16+, Ninja, a C++20 compiler, nlohmann-json 3.11+, ZLIB. Qt prefix defaults to `$HOME/Qt/6.11.1/gcc_64`; override with `make install QT_PREFIX=/path/to/Qt/6.x/gcc_64`.

Then run from anywhere:

```bash
lpcl help
```

### Install a package built on another machine

Run `make package-tar` on the build machine to produce the zero-dependency tarball `cli/dist/lpcl-linux-<arch>.tar.gz`, copy it to the target machine, then:

```bash
bash install.sh lpcl-linux-x86_64.tar.gz
```

(`install.sh` lives in `cli/` in the repo; it's also available from the release page.)

### One-liner

```bash
curl -fsSL https://github.com/recallrw80-afk/LPCL/releases/latest/download/install.sh | bash
```

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
# 1. Set the game folder (defaults to ./mc/ next to the binary; optional)
lpcl set-folder /home/yourname/mc

# 2. Import a modpack
lpcl inpack ~/Downloads/some-modpack.zip

# 3. See what instances you have
lpcl list

# 4. Launch!
lpcl launch
# or pick directly:
lpcl launch <instance-name>
```

No modpack? Install vanilla first:

```bash
lpcl mc-install          # latest release
lpcl mc-install 1.20.1   # specific version
lpcl launch 1.20.1
```

## Command reference

### Instances

| Command | Description |
|---|---|
| `list` | List all instances |
| `mods <name>` | List an instance's mods (file size + enabled state) |
| `list-rm [name\|*]` | Remove instances (interactive picker + confirmation without args; `*` removes all — quote it) |
| `mc-list` | List downloaded vanilla MC versions |
| `launch [name]` | Launch the game; without a name, an **arrow-key picker** appears (instances + vanilla/loader versions; falls back to numeric input on non-TTY) |

### Download & import

| Command | Description |
|---|---|
| `inpack <file> [--r <name>] [--to <instance>] [--folder <path>]` | Import a modpack; `--r` renames the instance; mod packs need `--to` for the target instance |
| `mc-install [version]` | Download a vanilla MC version (latest release without args) |
| `java-install <major>` | Download and install Java (Adoptium JRE) |

### Java

| Command | Description |
|---|---|
| `list-javas` | List detected Java installations |

### Players

| Command | Description |
|---|---|
| `player-add [name] [--avatar <path>] [--skin <slim\|wide\|default>]` | Add a player; interactive wizard without args (name → skin → avatar → advanced custom UUID) |
| `player-edit [uuid\|index]` | Edit a player (wizard, Enter keeps current value; interactive picker without args) |
| `player-rm [uuid\|index]` | Remove a player (interactive picker + confirmation without args) |
| `player-list` | List players (numbered, `*` marks the selected one) |
| `player-select <uuid\|index>` | Select the active player (index accepted; interactive picker without args) |

### Config & misc

| Command | Description |
|---|---|
| `set-folder <path>` | Set the default game folder |
| `set-lang <en\|zh>` | Set the UI language (English by default) |
| `set-mem <MB\|auto>` | Set max game memory; `auto` (default) allocates 50% of available RAM (capped at 16G) |
| `login [server] [email]` | External authlib-injector login (e.g. LittleSkin; token encrypted & persisted, auto-refreshed at launch) |
| `logout` | Log out the external account, falling back to the offline player |
| `update` | Check GitHub Releases and update in place (install.sh-installed copies only; override repo with `LPCL_REPO`) |
| `uninstall [-r]` | Uninstall the launcher; `-r` keeps game folder contents |
| `config` | Show current config |
| `report [description]` | Generate a prefilled GitHub Issue link (environment info + recent launch log attached, sanitized) |
| `test` | Full system self-check |
| `help` / `version` | Help / version |

## Where the game files live

By default in `mc/` next to the launcher (change with `set-folder`):

```
mc/
├── instances/      # each imported modpack instance (isolated)
├── versions/       # downloaded MC versions
├── libraries/      # game libraries (shared across instances)
├── assets/         # game assets (shared across instances)
├── javas/          # auto-downloaded Java runtimes
└── logs/           # launcher logs (lpcl-launch-*.log, last 10 kept)
```

The game's own log is at `<instance>/logs/latest.log` (standard MC/mod location); each `launch` also writes a full session log (launch command line + all game output + exit code) to `mc/logs/lpcl-launch-<timestamp>.log` — check that one for launch problems.

## FAQ

**Do I need an API key to import CurseForge modpacks?**
No. Official prebuilt packages embed a key (fastest, official API); source builds ship without one and automatically use the MCIM mirror. If a key expires or is revoked, requests automatically fall back to the mirror — nothing to handle manually.

**How do I switch the UI to Chinese?**
`lpcl set-lang zh` (persistent, one-time).

**Is there a GUI?**
The QML GUI is currently a test version sharing the same SDK as the CLI. Start it with `make run` from a dev build (binary name `lpcl-gui`); release packages containing the GUI register an `lpcl-gui` command.

**What happens when an import fails?**
Any failed download stage (game files / modloader / mods) rolls back everything — no half-installed instance is left behind; just retry.

## License & notice

Licensed under [GNU GPL v3](../LICENSE). This is independently developed open-source software: not affiliated with Mojang / Microsoft ("Minecraft" is a trademark of Mojang Synergies AB) and not affiliated with or authorized by the official [PCL](https://github.com/Hex-Dragon/PCL2). Provided "as is" — no liability accepted (see sections 15–16 of the license).
