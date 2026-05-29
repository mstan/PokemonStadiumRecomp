# Open issues — PokemonStadiumRecomp

Living list of known gaps. Use this as a pre-flight before starting
a new work session.

## Current playable build — known visible issues

The base game runs end-to-end (Quick Battle, Free Battle, Stadium
cups, Gym Leader Castle have been validated) but the following
visible imperfections remain.

**Priority order (user-set 2026-05-28):**
1. **Cursor / icon sprite corruption** — HIGHEST. Fix this next.
2. Diagonal line across the screen at transitions.
3. POKéMON STADIUM panel bottom clip.
4. Selected Game Pak card strip-overlay residual.
5. GB Tower "start GB game" crash — deep-dive, currently unreproducible
   (no GB cart simulated); higher than the latent gfx UAF, lower than
   the visible bugs above.
6. Latent Stadium-side gfx pool UAF / race — lowest (masked by RT64).

- [x] **Small clicking sound between audio chunks** during music
      sequences. **FIXED 2026-05-28** (runtime fork `N64ModernRuntime`,
      `librecomp/src/ai.cpp` — AI_LEN_REG emulation). Root cause was
      a forked-runtime bug: the Audio Interface length register was
      never emulated (always read 0), so the game's hardware audio
      pacing was dead and it over-produced audio; the host
      `skip_factor` decimation drained the surplus by dropping samples
      → ~4 clicks/s. Full post-mortem under
      [Audio → Music-rate periodic tick](#audio) below.

- [ ] **Visually corrupted "hand" pointer / cursor sprite** on
      certain menus. **★ HIGHEST PRIORITY (user-set 2026-05-28) — fix
      this next.** Re-confirmed 2026-05-28: on the Gym Leader Castle
      Registration screen the icon left of "Register Pokémon" renders
      as a sparkly/glittery garbled block where a clean icon should be
      (screenshot captured this session).
      Pattern: pointer texture renders with
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

      *2026-05-23 second update — the `memmap-get-fragment-data-context`
      hack retirement (PSR `9bf003c` + lib/N64ModernRuntime `3ed8bfd`)
      DID NOT resolve this bug, despite a speculative claim in that
      commit message. Cursor + icon corruption verified still present
      after the fix shipped. The hack retirement was a clean library-
      API promotion only — no functional behavior change at the
      Stadium-side resolver path. Cursor UAF remains outstanding;
      the proper-fix layer hypothesis (RT64 + memory-state) stands.*

- [x] **Line patterns through HUD elements across menus.** ~~Fixed
      2026-05-23 by bumping fragment-57 Vtx-grid seam overlap from
      +1 (Codex's original) to +2 in `extras.c`
      (`FRAG57_SEAM_OVERLAP` constant).~~ Properly retired
      2026-05-24 via conservative rasterization on RT64's N64 triangle
      PSO (lib/rt64 `b60cf10` + PSR `fde67c7`) — the renderer covers
      every pixel any triangle touches, eliminating the shared-edge
      tie-break that left dashed seams. The OVERLAP=2 hack + helper
      functions deleted; native geometry renders cleanly at every
      render scale. Verified on Game Pak Check (cards 2/3/4 + WARNING
      banner) and Main Menu icon panels (which reuse the same frag57
      grid layout via different MTX translations).

- [ ] **Faint horizontal residual streaks on the SELECTED Game Pak
      card's 5-strip overlay** (post fragment57-selected-card-overlay
      retirement, 2026-05-24). Card 1 at Game Pak Check now correctly
      shows the controller-icon graphic (previously hidden by the hack)
      but the 5-strip RGBA decoration behind the text shows faint
      horizontal yellow lines along strip boundaries. Conservative
      rasterization (which fixed the row-divider seam class) is
      neutral on this artifact — verified by A/B with
      `RT64_CONSERVATIVE_RASTER=0`. Likely a different bug class:
      TMEM row sampling at strip boundaries, narrow-strip s-coord
      precision, or texture-wrap-at-edge. Investigation deferred;
      not a regression from the prior state (the hack was net-negative,
      it omitted the controller icon entirely).

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

- [ ] **Diagonal line across the entire screen at transitions**
      (reported + screenshotted 2026-05-28). A thin straight line runs
      roughly corner-to-corner across the whole frame. Subtle and
      usually only visible during screen transitions/fades, but present.
      Observed both over a 3D model scene (Rhydon close-up) and over the
      main mini-game select screen — i.e. it's not tied to specific
      content, it's a full-frame overlay artifact.

      *Hypothesis:* the fullscreen transition/fade quad is drawn as two
      triangles, and a hairline gap along their shared diagonal
      (hypotenuse) edge shows the background through it — the same
      shared-edge tie-break seam CLASS that conservative rasterization
      fixed for the fragment-57 HUD grids (lib/rt64 `b60cf10`), but on
      the fullscreen transition quad, which evidently isn't covered by
      that fix (different PSO / coverage path, or a screen-space-aligned
      diagonal). Next step: identify the transition-quad draw in RT64
      and check whether conservative raster applies to its PSO; if the
      seam is the cause, extend coverage there. Low severity (subtle,
      transition-only) but a real renderer artifact.

- [ ] **GB Tower "start GB game" crash** (reported 2026-05-28; deep-dive,
      currently UNREPRODUCIBLE). Attempting to start a Game Boy game
      from within the embedded Game Boy Tower crashes. Cannot reproduce
      right now because no GB cartridge is being simulated into the
      Tower yet. Note: GB Tower is documented as **out of scope** in
      `CLAUDE.md` decision #4 (the plan was to stub its entry points,
      not port the embedded GB emulator) — this item revisits that as a
      future deep-dive, NOT current scope. Priority: above the latent
      gfx pool UAF below, but below the visible rendering bugs above.
      When picked up, first establish a reproduction (simulate/load a GB
      cart into the Tower path) before diagnosing.

- [x] **Active per-site workarounds in `extras.c` / `game.toml`
      should migrate to proper runtime fixes.** ~~Tracked in
      [`TEMP_PATCHES.md`](TEMP_PATCHES.md).~~ As of 2026-05-24,
      **all 8 active entries retired**:
      - `free-battle-modal-softlock` + `petit-cup-softlock` +
        `asset-pending-bypass` → ultramodern voluntary preemption
        (N64MR `b0364f2` host-monitor + yield flag).
      - `fragment57-vtx-seams` → conservative rasterization on the
        RT64 N64 triangle PSO (lib/rt64 `b60cf10`).
      - `fragment57-selected-card-overlay` → un-ignored, native code
        emits the controller icon correctly (residual strip-overlay
        artifact tracked above as a separate visible bug).
      - `audio-uaf-fragment36-voice-stop` → librecomp
        `SecondaryVoiceTableLayout` (PSR `cefd9f6`).
      - `force-expansion-ram` → `GameEntry::on_init_callback`
        (PSR `43a2519`).
      - `memmap-get-fragment-data-context` → librecomp helper API
        (PSR `9bf003c` + N64MR `3ed8bfd`).

- [ ] **Latent Stadium-side gfx pool UAF / race** (newly named
      2026-05-23 — RT64 fix masks the symptom). The attract-demo
      softlock root-cause was Stadium emitting a `G_DL CALL` into
      memory at `0x80208F18` that holds compressed audio/image
      bytes instead of DL commands. The RT64 fix prevents the
      renderer from softlocking on garbage data, but Stadium's
      pool/allocator is still emitting CALLs to wrong addresses.
      Likely the same family as the prior audio UAF (where freed
      wavetables get repurposed). Symptom may be invisible if no
      one walks the garbage area as a DL anymore, but a future
      bug could surface from the same broken pool management.
      Proper investigation requires the Ares oracle bridge to
      diff RDRAM at the moment Stadium emits the bad CALL.

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

- [x] **Music-rate periodic tick — FIXED 2026-05-28.** Forked-runtime
      bug: the N64 Audio Interface length register (`AI_LEN_REG`) was
      never emulated, killing the game's hardware audio-pacing feedback
      loop, causing audio over-production that the host sample-decimation
      drained audibly (~4 clicks/s). Fixed by emulating the register in
      `lib/N64ModernRuntime/librecomp/src/ai.cpp`.

      **POST-MORTEM (the full chain).**

      1. *Symptom.* Rhythmic ~4/s click during all music (never SFX or
         speech), "at the boundary of audio packets," present in
         real-time playback. Mis-described early on as tempo-tracking;
         it is actually a near-constant rate that is merely more audible
         over busy passages.

      2. *Mechanism (proximate).* `src/main/main.cpp` `queue_samples()`
         has a `skip_factor` valve: when the SDL output queue exceeds
         100ms it decimates the chunk (keeps every `1<<skip_factor`-th
         sample). Dropping samples mid-waveform = a discontinuity =
         click. Confirmed via an always-on host-queue ring
         (`audio_queue_recent`): in real-time the queue sat at 90-105ms,
         straddling the 100ms threshold, and ~81/1200 chunks were
         decimated ≈ 4.0/s.

      3. *Why the queue was full (root cause).* The game (standard
         libultra audio manager, `disasm/src/3D140.c` `func_8003CADC`)
         paces generation by reading `HW_REG(AI_LEN_REG)` — the bytes
         still pending playback — and emitting a SHORT frame
         (`minFrameSize`) when the buffer is full, a full frame
         (`frameSize`) when low. But the runtime maps the MMIO region to
         zero pages (`librecomp/addresses.hpp`), so `AI_LEN_REG` ALWAYS
         read 0. `samplesLeft >= 0x1A9` was never true → the game ALWAYS
         emitted full frames → it generated 60×552 = 33120 frames/s vs
         32000 consumed (~3.5% over) with ZERO back-pressure.

      4. *Decisive experiment.* Added `PSR_AUDIO_NO_DECIMATE=1`. With
         decimation off the clicks vanished but the SDL queue grew
         perfectly linearly, unbounded — 85ms → 3.14s and climbing
         (~50ms/s). Linear unbounded growth = fixed over-production with
         no feedback (NOT a mistuned threshold), proving the decimation
         was load-bearing and the real defect was upstream pacing.

      5. *Rejected hypotheses (all instrumented, runtime-side,
         hook-free).* Stale ADPCM predictor carry / voice-reuse: an
         ADPCM-decode command-list scan (`adpcm_decode_recent`) showed
         SUSPECT=0 — the predictor is reset (A_INIT) on every sample
         change. Voice-event and SP-task rings corroborated: note
         attacks ~15-23/s, chunk rate fixed 60/s. The decoder path was
         provably clean; the bug was never voice-side.

      6. *Fix.* Emulate `AI_LEN_REG` in `lib/N64ModernRuntime/librecomp/
         src/ai.cpp`: after each `osAiSetNextBuffer` (and in
         `osAiGetLength`), publish the live remaining-byte count
         (`ultramodern::get_remaining_audio_bytes()`) into the register
         via `MEM_W` at the same host address the game's `HW_REG` read
         resolves to (kseg1 `0xA4500004`). This restores the game's own
         hardware-designed feedback loop. **Generic across every
         libultra audio game** — not Stadium-specific.

      7. *Verification.* Real-time, post-fix: chunk rate dropped
         60/s → 57.4/s (the game now inserts short frames, matching the
         32000/552 ≈ 58/s hardware rate); queue depth fell from ~100ms
         to a bounded ~0-35ms (avg ~16ms, the game's ~13ms target);
         **decimation count = 0** over 26s; user confirmed no clicks by
         ear. The `skip_factor` decimation remains in place as a dormant
         runaway safety net (never fires under normal pacing).

      **Lesson / class of bug.** Unemulated MMIO registers that default
      to 0 can silently disable a game's hardware feedback loops, with
      effects that surface far downstream (here, an audio artifact 3
      layers away in the host output path). The fix belongs in the
      runtime fork and benefits all future games.

      **Diagnostic instrumentation added (kept; always-on, env-gated,
      no game.toml hooks):** `voice_events_recent`, `adpcm_decode_recent`
      (librecomp `audio_uaf_protect.cpp`), `audio_queue_recent`
      (`main.cpp`); env gates `PSR_DISABLE_VOICE_RING`,
      `PSR_DISABLE_AUDIO_QUEUE_RING`, `PSR_AUDIO_NO_DECIMATE`; readers
      `tools/_voice_events.py`, `_adpcm_decode.py`, `_sp_audio.py`,
      `_audio_queue.py`. Full trail in memory
      `project_music_click_hypothesis_2026_05_28.md` +
      `reference_npvoice_offsets.md`.

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
