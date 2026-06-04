# PokemonStadiumRecomp

Static recompilation of **Pokémon Stadium (US v1.0)** to native PC.
Built on top of [N64Recomp](https://github.com/N64Recomp/N64Recomp).

> **Status: Early development — playable end-to-end, with known
> issues.** Quick Battle, Free Battle, Stadium cups, and Gym Leader
> Castle have all been validated through a complete round, and both the
> Transfer Pak party-import path and the embedded GB Tower now work. It
> is still rough: expect occasional audio crackle, visual glitches on
> some menu elements, and a softlock in some menus. See
> [`ISSUES.md`](ISSUES.md) for the current list.
>
> **First launch:** the runner pops a file picker. Point it at your
> own legal Pokémon Stadium (US v1.0) ROM (`.z64` / `.n64` / `.v64`).
> The path is remembered (`rom.cfg` next to the exe) so later
> launches go straight into the game. CLI arg `argv[1]` is also
> honored for scripted runs. Audio is muted by default during
> development — set `PSR_VOLUME=1.0` (any value 0–1) to enable sound.

## Features

- **Plays Pokémon Stadium end-to-end** — Quick Battle, Free Battle, the
  Stadium cups, and Gym Leader Castle have each been run to completion.
- **GB Tower — play full Game Boy games inside Stadium, with your own
  ROMs and saves.** The embedded Game Boy emulator launches DMG and CGB
  carts (Red, Blue, and Yellow verified to gameplay); cart save load and
  write are verified for MBC3 and MBC5, and GB audio is routed to the
  host. Provide your own legal GB/GBC ROM + save — see
  [GB Tower](#gb-tower) below.
- **Transfer Pak — import your party from a Game Boy cart.** A
  hardware-level Transfer Pak + GB cart emulator (ROM-only / MBC1 / MBC3
  / MBC5) feeds the in-game Game Pak Check / Registration menus from your
  own ROM + save. Battery SRAM is persisted on every write, and
  registered Pokémon now persist across runs (Stadium FlashRAM save).
  Multiple controllers/Transfer Paks (ports 1–4) are supported. See
  [Transfer Pak](#transfer-pak) below.
- **RT64-backed rendering** with internal-resolution upscaling.
- **Audio follows your Windows default output device** and migrates live
  when you change it; override with `PSR_AUDIO_DEVICE=<name substring>`.
- **SDL game-controller input** (including DualSense/DualShock).
- **Configurable** via env vars and `*.cfg` files placed next to the exe
  (`rom.cfg`, `transfer_pak.cfg`).

## ROM

| Field | Value |
|-------|-------|
| Title | Pokémon Stadium (US, v1.0) |
| MD5   | `ed1378bc12115f71209a77844965ba50` |
| Size  | 33,554,432 bytes (32 MB) |
| Format | `.z64` (big-endian native, magic `80 37 12 40`) |

**Rev A (v1.1) is not compatible** — pret's disassembly targets v1.0
specifically, and the address tables in `disasm/yamls/us/rom.yaml`
will not align with a Rev A binary. If you have Rev A, find a v1.0
dump.

## Layout

```
PokemonStadiumRecomp/
├── baserom.z64                     # canonical ROM (gitignored)
├── disasm/                         # pret/pokestadium submodule
├── n64recomp/                      # N64Recomp engine (junction by setup)
├── ares-bridge/                    # Ares oracle integration (TODO subproject)
├── ghidra/                         # Ghidra project + instructions
├── generated/                      # recompiler C output (gitignored)
├── tools/                          # game-specific tooling
├── tests/                          # regression tests
├── docs/                           # design notes
├── game.toml                       # N64Recomp config
├── n64recomp.pin                   # engine SHA pin
├── CMakeLists.txt                  # build entrypoint
├── setup.sh / setup.bat            # provisioning
├── DEBUG.md                        # divergence triage protocol
├── ISSUES.md / MODDING.md
└── README.md
```

## Quick start

Prereqs: git, python3, cmake 3.20+, a working C/C++ toolchain. For
the disasm build (optional): `make`, `binutils-mips-linux-gnu`.

```bash
# Linux / macOS
chmod +x setup.sh && ./setup.sh

# Windows
setup.bat
```

This:
1. Clones (or junctions) `n64recomp/` at the SHA pinned in
   `n64recomp.pin`.
2. Initializes the `disasm/` submodule (pret/pokestadium).
3. Stages `baserom.z64` into `disasm/baseroms/us/`.

Optional: clone the Ares oracle with `WITH_ARES=1 ./setup.sh`. See
the *Oracle* section below — the bridge code is not yet written.

To build the disasm (sanity check that the ROM and pret align):

```bash
cd disasm
make init     # extracts assets from the staged baserom
make          # rebuilds an identical pokestadium-us.z64 from sources
```

## Transfer Pak

Stadium reads your Pokémon party out of a Game Boy cart through the
N64 Transfer Pak accessory. This runtime ships a hardware-level
emulator of that accessory plus the Game Boy cart it bridges to, so
the in-game Game Pak Check menu sees the configured ROM/save as if
a real cart were plugged into a Transfer Pak in port 1.

**Supported cart types:** ROM-only, MBC1, MBC3, MBC5 (covers Pokémon
Red / Blue / Yellow / Gold / Silver / Crystal). Battery-backed SRAM
is persisted to disk on every write.

**Configuration.** Drop a `transfer_pak.cfg` next to the exe:

```ini
# Paths are relative to this file (or absolute).
p1_rom=pokemon-yellow.gbc
p1_save=pokemon-yellow.sav
```

Keys are `pN_rom` / `pN_save` for ports 1–4. Environment variables
`PSR_TRANSFER_PAK_P{1..4}_ROM` and `..._SAVE` override the config
file. Set `PSR_TRANSFER_PAK_DEBUG=1` for verbose bus-level tracing
(off by default).

`transfer_pak.cfg` and `*.gb` / `*.gbc` are gitignored — bring your
own legal dumps.

## Pipeline overview

```
disasm/  +  baserom.z64    -->  pret build      -->  pokestadium-us.elf
                                  (make init && make)         |
                                                              v
                          game.toml  +  N64RecompCLI  -->  generated/*.c
                                                              |
                                                              v
                                  CMake build  +  N64ModernRuntime
                                                              |
                                                              v
                                                  PokemonStadiumRecomp.exe
```

The disasm produces an ELF that already encodes every section's
load VA, symbols, and relocations. **N64Recomp consumes the ELF
directly** — there's no per-fragment slicing step. Ghidra also
imports the ELF directly (see `ghidra/instructions.txt`).

## Overlays — flat VA, not NES-style banks

Pokémon Stadium has a flat 8 MB virtual address space and uses
DMA-loaded *fragments* (overlays). Verified from
`disasm/yamls/us/rom.yaml`:

- 77 numbered fragments at **mostly unique VRAM addresses**
  (`0x81200000`, `0x87800000`, `0x87900000`, `0x8F000000`, …).
- Only **2 placeholder VRAM collisions** (`0x8FC00000` ×2,
  `0x88920000` ×2), commented in pret as "unk VRAM, shuts linker
  up" — likely not real runtime collisions.

This is **unlike NES bank-switching** (where every bank shares
`0x8000-0xFFFF`). For PokemonStadium the disasm's ELF is sufficient
for both N64Recomp and Ghidra; per-fragment extraction is not
needed and the project doesn't ship that tooling.

## GB Tower

Stadium's built-in **GB Tower** is the embedded Game Boy emulator that
plays full GB/GBC cartridges directly on the N64 — distinct from the
Transfer Pak (which imports a *party* into Stadium; see above). It is
**supported**: Red, Blue, and Yellow have been launched to live gameplay,
DMG and CGB boot paths both work, cart save load/write is verified
(MBC3 / MBC5), and the embedded emulator's audio is routed to the host.

GB Tower reads carts through the same Transfer Pak cart model, so you
supply your own legal GB/GBC ROM + save the same way — via
`transfer_pak.cfg` (`p1_rom` / `p1_save`) or the
`PSR_TRANSFER_PAK_P{1..4}_ROM` / `..._SAVE` environment variables. In the
game: **POKéMON STADIUM → right to the giant Game Boy (GB Tower) → pick a
cart → A**. The 1P slot is the default cart; additional ports map to the
2P/3P carts.

`PSR_DISABLE_GBTOWER_AUDIO=1` is available as a diagnostic opt-out for
the GB audio path.

## Out of scope (first pass)

Nothing major remains explicitly out of scope for the base game at this
point — GB Tower (above) was the last big "out of scope" item and is now
in. Remaining gaps are tracked as ordinary issues in
[`ISSUES.md`](ISSUES.md) rather than scope exclusions.

## Oracle (Ares) — TODO

N64Recomp does not ship oracle infrastructure the way nesrecomp
ships with Nestopia. Wiring Ares as a divergence-checking oracle is
a follow-up subproject. The slot is reserved at `ares-emulator/`
(opt-in via `WITH_ARES=1` in setup); the bridge code (`ares_bridge.cpp`,
`n64_snapshot.c`, `verify_mode.c`, `watchdog.c`) is not yet
written. Track in `ISSUES.md`.

## Documentation

- [`DEBUG.md`](DEBUG.md) — debug + divergence protocol.
- [`ISSUES.md`](ISSUES.md) — known issues + open work.
- [`MODDING.md`](MODDING.md) — modding hooks (post-MVP).
- [`ghidra/instructions.txt`](ghidra/instructions.txt) — Ghidra setup.

## Credits

- pret/pokestadium — disassembly
- N64Recomp by Mr-Wiseguy and contributors — recompiler
- ares-emulator team — accuracy reference
