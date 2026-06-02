# PokemonStadiumRecomp

Static recompilation of **Pokémon Stadium (US v1.0)** to native PC.
Built on top of [N64Recomp](https://github.com/N64Recomp/N64Recomp).

> **Status: Playable end-to-end (with known visible bugs).** Quick
> Battle, Free Battle, Stadium cups, and Gym Leader Castle have all
> been validated through a complete round. Audio plays at 50% by
> default (`PSR_VOLUME=0.5`). See [`ISSUES.md`](ISSUES.md) for the
> remaining visible glitches (small clicks during music, a
> corrupted cursor on a few menu screens, gridline patterns through
> certain HUD elements).
>
> **First launch:** the runner pops a file picker. Point it at your
> own legal Pokémon Stadium (US v1.0) ROM (`.z64` / `.n64` / `.v64`).
> The path is remembered (`rom.cfg` next to the exe) so later
> launches go straight into the game. CLI arg `argv[1]` is also
> honored for scripted runs.
>
> **Transfer Pak is supported** via a built-in Game Boy cart
> emulator (ROM-only / MBC1 / MBC3 / MBC5, with battery RAM saves).
> Point at your own legal GB/GBC ROM via `transfer_pak.cfg` — see
> the [Transfer Pak](#transfer-pak) section below. GB Tower (the
> embedded full GB emulator for free-play GB games inside Stadium)
> remains out of scope.

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

## Out of scope (first pass)

**GB Tower** — the embedded Game Boy emulator that lets Stadium
play full GB cartridges directly on the N64 (distinct from the
Transfer Pak cart-bridge, which **is** supported — see above). The
GB-emulator code lives partly in the resident `text` segment
(symbols `gb_tower`, `gb_mbc`, "load gb emulator" near
`0xB230`/`0xE1C0`/`0xE570`) and partly in the `gb_tower_roms` tail
segment. GB Tower entry points are **stubbed**, not ported — see
`ISSUES.md`.

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
