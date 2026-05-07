# Open issues — PokemonStadiumRecomp

Living list of known gaps. Use this as a pre-flight before starting
a new work session.

## Scaffolding (current phase)

- [ ] Replace placeholder SHA in `n64recomp.pin` with the real
      40-char HEAD of `../N64Recomp` after `setup.sh` runs. Currently
      pinned to a placeholder padded with hex.
- [ ] Verify `setup.sh` and `setup.bat` end-to-end on a clean clone.
- [ ] First `disasm/make init` + `disasm/make` run — confirm pret
      builds identically against `disasm/baseroms/us/baserom.z64`.

## Recompilation pipeline (next phase)

- [ ] **Build pret's disasm to produce `pokestadium-us.elf`.**
      Requires MIPS binutils + make + python. On Windows this
      means WSL2 — full instructions in `docs/disasm-build.md`.
- [ ] Wire `game.toml` to point at the produced ELF
      (`disasm/build/pokestadium-us.elf`).
- [ ] First N64Recomp run against the ELF. Expect warnings on
      indirect call sites and unmapped relocations — triage them.
- [ ] CMake target that invokes `N64RecompCLI` and depends on
      the disasm build.
- [ ] Decide what to do with the 2 placeholder-VRAM fragment
      collisions (`0x8FC00000`, `0x88920000`) — likely nothing
      until they cause an analysis or recomp warning.

## Runtime (depends on N64ModernRuntime)

- [ ] Vendor or submodule N64ModernRuntime (the runtime that
      Zelda64Recomp / MM:R consume).
- [ ] Wire the runner CMake target — link `generated/*.c` against
      runtime + SDL2.
- [ ] First boot — expect to crash or blackscreen; record the
      first divergence.

## GB Tower (out of scope, first pass)

- [ ] Enumerate all GB-related entry points reachable from
      resident `text`. Symbols of interest: `gb_tower`, `gb_mbc`,
      addresses near `0xB230`, `0xE1C0`, `0xE570`.
- [ ] Add `[[patches.stubs]]` entries in `game.toml` for each.
- [ ] Confirm main game (battles, minigames) reachable without
      hitting any stub.

## Oracle / divergence checking

The Ares oracle bridge lives in the **engine**, not in this
project — see `n64recomp/ares-bridge/` and its
`README.md` / `DESIGN.md`. PokemonStadiumRecomp consumes the
bridge as a CMake target.

Engine-side work (tracked on the `pokestadium-integration` branch
of N64Recomp):
- [ ] Carve out a buildable, headless Ares N64 core
      (`ares-bridge` Phase 1).
- [ ] Implement lifecycle + state read + step
      (Phases 2-4).
- [ ] Validate the bridge against Zelda64Recomp running OoT
      (Phase 5) **before** turning it on for this project.

Consumer-side work (this project):
- [ ] Wire `target_link_libraries(PokemonStadiumRecomp PRIVATE
      ares_bridge)` in CMakeLists.txt once the runner target
      exists.
- [ ] `verify_mode.c/h` — runtime mode that boots, runs N frames,
      diffs state via `ares_read_*`, exits. Pattern matches
      Faxanadu's `verify_mode.c` against Nestopia.
- [ ] `n64_snapshot.c/h` — local snapshot format. Layout chosen
      to match the byte order Ares returns from
      `ares_read_memory()`.
- [ ] `watchdog.c/h` — divergence assertion firing on register
      or memory mismatch reported by the bridge.

## Modding (deferred — see proposals)

Explicitly **not** prioritized until the base game boots and is
playable.

- [ ] Mod manifest schema.
- [ ] RecompModTool / OfflineModRecomp integration.
- [ ] RecompModMerger.
- [ ] LiveRecomp / sljit JIT — almost certainly skipped.

## Documentation

- [ ] `MODDING.md` — flesh out post-MVP. Currently a placeholder.
- [ ] `docs/` — design notes on overlay handling, GB Tower
      stubbing, oracle architecture.
- [ ] Per-overlay README skeletons under `overlays/<name>/` once
      extraction tool runs.

## Audio

- [ ] **Music-rate periodic tick.** After the post-title audio UAF
      fix landed (2026-05-06), a subtle periodic tick remains during
      music playback. User reports it tracks the music tempo (not
      sound effects, not announcer/dialog), suggesting either a
      sequence-tick-rate or chunk-DMA-boundary artifact. Diagnostic
      rings are quiet (no `[adpcm-overflow]`, no `[dispatch-corrupt]`,
      no `[aspMain]` unknown-dispatch) — this is a sub-catastrophic
      DSP-level fidelity issue, not the same UAF family.

      Hypotheses, in priority order:
      1. **aspMain chunk-N DMA boundary.** The pre-task hook now
         correctly handles chunk 0; chunk-N transitions (each subsequent
         0x140-byte fill from DRAM into DMEM[0x2B0]) have not been
         instrumented. An off-by-one at a chunk boundary would
         manifest as a click roughly every (chunk_count × frame_rate),
         which fits "music-rate" if the audio task processes
         multiple chunks per frame.
      2. **Voice re-allocation glitch.** When Stadium starts a new
         voice in a new scene, there may be a brief moment where
         `dc_table` is set but `dc_state` (per-voice ADPCM
         decompressor state) hasn't been initialized. The first
         ADPCM block plays from uninitialized state and clicks
         before settling.

      Next steps: instrument chunk-N DMA boundaries (extend the
      `[aspmain_chunk0]` ring to cover later chunks), correlate
      tick frequency with `n_alAudioFrame` cmd buffer size +
      chunk count.

- [ ] **Pre-decompressed ROM build step.** Backlog architectural
      cleanup: replace runtime `[[input.decompressed_section]]`
      engine extension with a build-time tool that produces a fully
      decompressed Stadium ROM, then recompiles against that. Wins:
      simpler engine, recompiler can see post-decompression bytes
      directly, mods target known offsets. Cost: bigger ROM (32 MiB
      → ~80–100 MiB), pre-decompression tool needs to handle Yay0
      and PERS-SZP fragments. Existing infrastructure to build on:
      `tools/_decompress_fragment78.py`. Defer until the audio
      family is fully closed.

- [ ] **kseg1 ROM-read patching.** Backlog architectural cleanup:
      replace runtime `mirror_rom_to_kseg1` (currently populates
      `rdram[0x30000000..]` with ROM bytes for direct cart-vaddr
      reads) with per-site `game.toml` patches at each known
      kseg1-ROM-access site. Known site: `Game_DoCopyProtection`
      reading `*(u32*)0xB0000E38`. Wins: keeps the rdram region
      free for mod use. Defer until the audio family is fully closed.
