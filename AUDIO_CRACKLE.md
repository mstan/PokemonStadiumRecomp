# AUDIO_CRACKLE — PMS-USA recompiled-synthesis crackle (open)

Handoff for a focused audio investigation. Self-contained; read this first, then
the "Tools" and "Recommended approach" sections before touching code.

## The bug

Pokémon Stadium (USA) has an audible **crackle** during audio playback,
confirmed by ear on 2026-07-02 (still present). It is content-dependent — the
attract-loop / menu music barely stress it; battle cries and SFX are the loudest
offenders. Reproduce: launch, drive into a battle, listen to a cry.

This is the ONE remaining audio defect. Do not confuse it with three *already
fixed* (different) issues — if you find yourself back on these, you're off track:
- Host "click" from unemulated `AI_LEN_REG` → no pacing → over-production
  drained by decimation. FIXED in N64ModernRuntime `ai.cpp` (2026-05-28).
- DualSense "static" = audio-device routing. FIXED via `PSR_AUDIO_DEVICE`.
- Turbo/fast-forward decimation clicks. FIXED (turbo default off).

## What is already ruled OUT (do not re-litigate)

- **Host output path is clean.** Decimation / resampler / device were measured
  clean end-to-end. The crackle is NOT introduced after the guest hands off PCM.
- **Vector unit is exonerated.** A SISD-vs-SIMD A/B of the synthesis both
  crackle identically → it is not a vectorization bug.
- Therefore the defect is **upstream, in the guest-side synthesis**:
  the **recompiled `aspMain` RSP microcode PCM synthesis**. The crackle is baked
  into the samples the guest produces, before host playback.

## Prime suspects (in the recompiled RSP synthesis path)

1. **RSPRecomp codegen** — `N64Recomp/RSPRecomp/src/rsp_recomp.cpp` emits the C
   for `aspMain`. A subtly wrong opcode lowering (saturation/rounding/clamp on an
   accumulator op, or a DMEM addressing edge) would corrupt specific samples.
2. **RSP DMA** — `N64ModernRuntime/librecomp/src/rsp.cpp`. A byte-count / stride
   / wraparound error on the sample or ADPCM-book DMA corrupts the input buffers.
3. **libnaudio glue** (shared with PS-US) — buffer sizing / voice mixing seams.

Rank by evidence from the rings before editing anything.

## Tools & ALWAYS-ON rings (consume these; never arm-then-run)

Debug server is always up on `tcp:4371` (`PSR_DEBUG_PORT` overrides). Send
newline-delimited JSON, e.g. `{"cmd":"audio_pcm_recent","n":256}`. Relevant
always-on ring queries (query the window you care about — do NOT pause/step):
- `rsp_dma_recent` — RSP DMA events (enable capture with `PSR_RSP_DMA_TRACE=1`).
  Fields incl. byte_count, dram/dmem addr, first-nonzero, first16 bytes.
- `voice_events_recent` — per-voice synthesis events (N_PVoice / ALWaveTable).
- `adpcm_decode_recent` — ADPCM decode events (the usual crackle culprit).
- `audio_queue_recent` — queued audio commands.
- `audio_pcm_recent` — the produced PCM window (the smoking gun: inspect for
  discontinuities / clipping / NaN-equivalent spikes at crackle onset).

Source: `src/main/audio_host_probe.cpp`, `src/main/recomp_audio_debug.h`,
`src/main/recomp_audio_drc.h`, `src/main/gb_audio.cpp`. A small TCP client is at
`<scratchpad>/dbg_query.py` (or write your own — one socket, one line).

## Recommended approach (differential, ring-driven)

Mirror the co-sim philosophy (`COSIM.md`): don't guess, **diff against truth**.
1. Capture the recompiled `aspMain` output for a crackling cry via
   `audio_pcm_recent` / `adpcm_decode_recent` — get the exact sample indices
   where the waveform discontinues (the crackle).
2. Build/reuse a **reference RSP interpreter** run of the SAME `aspMain` task on
   the SAME DMEM/DRAM inputs (the N64Recomp RSP path or the Ares oracle's LLE RSP
   can serve as ground truth), and diff its PCM output sample-by-sample against
   the recompiled output.
3. The first diverging sample localizes to a specific RSP op / DMA / voice.
   Walk it back to the codegen or DMA site. Fix the root cause in RSPRecomp or
   `rsp.cpp` — NOT a post-hoc smoothing of the output.
4. Confirm by ear + a clean PCM diff on the same cry.

The Ares oracle bridge (`N64Recomp/ares-bridge/`, `ares-bridge: real`) already
exposes an LLE RSP; it may be the fastest ground-truth source for step 2.

## Hard rules (same discipline as the rest of the tree)

- **No stubs** to make it build; declare needed stubs in `game.toml`
  `[[patches.stubs]]`. **Never edit `generated/`** (fix `game.toml` or the
  recompiler, then regen via `N64Recomp.exe game.toml`). **Never edit `disasm/`**.
- **Root-cause the synthesis**, don't mask the output. A DSP band-aid on the host
  side is explicitly out of scope — the host path is already proven clean.
- **Always-on ring buffers only** — query the ring for the window of interest;
  never arm-a-trace-then-run, never pause/step to synchronize observers.
- **Throttle every build** — use `build_ssanne.bat` / `build_cosim.bat`
  (`PSR_BUILD_JOBS` cap + below-normal priority). Never an un-throttled build.
- **Verify hashes**: `baserom.z64` MD5 `ed1378bc12115f71209a77844965ba50`;
  `n64recomp.pin` vs engine HEAD.
- **Leave PMS-J and its transfer-pak branches ALONE.** PMS-USA only here.
- **No upstream / Wiseguy / ecosystem references** in any public-facing material.
- The agent launches the game (`Start-Process` the exe, redirect stderr); the
  human drives input in-game. `PSR_AUTOBOOT=1` black-screens — never diagnose a
  black screen via autoboot; boot via the launcher PLAY.

## Build & run

```
# Throttled build (real cmd.exe; do NOT run the .bat under bash/PowerShell-as-noop)
& "$env:SystemRoot\System32\cmd.exe" /c "F:\Projects\n64recomp\PokemonStadiumRecomp\build_ssanne.bat"
# Launch (agent launches; human drives)
Start-Process F:\Projects\n64recomp\PokemonStadiumRecomp\build\PokemonStadiumRecomp.exe `
  -WorkingDirectory F:\Projects\n64recomp\PokemonStadiumRecomp\build `
  -RedirectStandardError err.txt
# Enable DMA trace first if you need rsp_dma_recent:  set PSR_RSP_DMA_TRACE=1
```

## Where to work

Branch off `main` (NOT `cosim/tier0` — that's the parked co-sim work). Suggested:
`work/audio-crackle`. Repos likely touched: N64Recomp (`RSPRecomp/`),
N64ModernRuntime (`librecomp/src/rsp.cpp`, libnaudio glue). Commit after each
win; credit the models involved.
