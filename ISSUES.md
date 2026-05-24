# Open issues — PokemonStadiumRecomp

Living list of known gaps. Use this as a pre-flight before starting
a new work session.

## Current playable build — known visible issues

The base game runs end-to-end (Quick Battle, Free Battle, Stadium
cups, Gym Leader Castle have been validated) but the following
visible imperfections remain:

- [ ] **Small clicking sound between audio chunks** during music
      sequences. Tempo-tracking, only on music (not SFX, not
      announcer / dialog). Sub-catastrophic DSP fidelity issue;
      detailed hypothesis + diagnostic next-steps recorded under
      [Audio → Music-rate periodic tick](#audio) below.

- [ ] **Visually corrupted "hand" pointer / cursor sprite** on
      certain menus (e.g. Gym Leader Castle Registration's
      "Register Pokémon" entry — sparkly/glittery sprite where a
      clean icon should be). Pattern: pointer texture renders with
      garbled bytes that worsen across repeated entries to the same
      screen, suggesting an accumulating-stale-state UAF in the
      cursor/icon allocator. Same screen used to soft-lock after
      a few entries — that hard crash is now fixed (n64recomp
      MEM_W mask 2026-05-10) but the visual glitch remains.

      *2026-05-23 update — the corruption is intermittent and
      affects different sprites on different visits, not just the
      cursor. Across three captures of the Gym Leader Castle
      screens:*
      - *Visit A: cursor garbled (rainbow stripes where the hand
        should be), top-left pokeball icon ALSO garbled.*
      - *Visit B: cursor garbled (different garbled pattern), top
        pokeball icon renders CORRECTLY.*
      - *Register Pokémon submenu: cursor renders cleanly as a
        hand, BUT the top-corner pokeball icon is replaced with a
        wholly-wrong brick/woven texture — different texture got
        blitted into TMEM where the pokeball should be.*

      *Different sprites corrupting on different visits strengthens
      the "allocator UAF / TMEM stale-state" hypothesis — the
      corruption isn't tied to a specific sprite, it's whichever
      sprite happens to inherit freed allocator slots / stale TMEM.
      Likely shares a layer with the attract-demo softlock
      (memory `project_attract_softlock_2026_05_23.md`) and the
      STADIUM panel bottom clip — all three smell like RT64 +
      memory-state issues that the Ares oracle could disambiguate.*

- [x] **Line patterns through HUD elements across menus.** ~~Fixed
      2026-05-23 by bumping fragment-57 Vtx-grid seam overlap from
      +1 (Codex's original) to +2 in `extras.c`
      (`FRAG57_SEAM_OVERLAP` constant). Lines were caused by RT64
      exposing single-pixel boundary rows between adjacent Vtx
      quads in the same panel; +1 source-unit overlap wasn't enough
      at the user's render scale, +2 fully clears.~~ Verified on
      Game Pak Check (all 4 cards) and main-menu icon panels
      (which reuse the same frag57 grid layout via different
      MTX translations).

- [ ] **POKéMON STADIUM panel bottom clip (main menu).** The
      central icon's "POKéMON STADIUM" label band gets the
      descenders cut off and the gap before the bottom corner
      bracket is too small. Different bug class from the seam
      issue — quad #7 of the 2×8 grid at frag57+0x8870 is
      intentionally short (13 units tall vs 20 for the column),
      and bumping seam overlap doesn't help. Likely Y-axis
      scissor / projection clipping, or the label texture's t-coord
      mapping extends past the quad's bottom. See
      `NOTES_TO_CODEX.md` for the investigation path.

- [ ] **Active per-site workarounds in `extras.c` / `game.toml`
      should migrate to proper runtime fixes.** Tracked in
      [`TEMP_PATCHES.md`](TEMP_PATCHES.md) — currently 2 active
      entries (`free-battle-modal-softlock`, `petit-cup-softlock`),
      both expected to be retired together when N64ModernRuntime /
      ultramodern grows voluntary preemption of stuck game threads.

- [x] **Attract-demo white-screen softlock.** ~~Fixed 2026-05-23
      in RT64 (`hle: harden interpreter against malformed G_DL +
      step watchdog`, `lib/rt64` commit `b5d5980`). Root cause:
      Stadium's attract-demo path emits a G_DL CALL into a buffer
      at `0x80208F18` that contains compressed audio/image bytes,
      not a DL. The 76th byte happens to be `0xDE` (G_DL opcode);
      RT64's interpreter treats it as a real G_DL, resolves the
      garbage w1 to a host pointer past 8 MiB RDRAM, and walks
      unbounded host memory forever. RT64 now rejects malformed
      G_DLs (pad bits non-zero) and OOB targets, plus a 16M
      step-watchdog as a backstop. The underlying Stadium-side
      UAF/race (emitting a CALL to memory that's been repurposed)
      still exists but no longer softlocks the renderer — the
      bad task completes with whatever was drawn before the bad
      command, dp_complete fires, game continues.~~ Memory note:
      `project_attract_softlock_2026_05_23.md`.

### Next-step gate: Ares oracle bridge

The three open visible bugs above each have multiple plausible
root-cause layers (recompiler emit, librecomp shim, RT64 renderer,
pret disasm correctness, or genuine game-side UAF that real N64
hardware also has). Static-analysis triage can narrow but not
eliminate — we keep ending up with two or three viable hypotheses.

The deterministic decider is the **Ares oracle workflow**
documented in [`DEBUG.md`](DEBUG.md): run Ares against the
original ROM and our recomp build to the same frame, byte-diff
RDRAM, find the first divergence, then trace backward to whether
the responsible write came from recompiled code (→ check pret's
claim about that address), from a librecomp shim (→ our libultra
emu bug), or from RT64 (→ renderer bug). The bridge is implemented
(see `n64recomp/ares-bridge/`) but currently has an in-process
blocker; the recommended next step is the **out-of-process
oracle** variant. Until that lands, the open bugs are
diagnosis-blocked at the "which layer" question and we're patching
symptoms instead of roots.

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
