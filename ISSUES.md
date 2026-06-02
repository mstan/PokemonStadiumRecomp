# Open issues — PokemonStadiumRecomp

Living list of known gaps. Use this as a pre-flight before starting
a new work session.

## Current playable build — known visible issues

The base game runs end-to-end (Quick Battle, Free Battle, Stadium
cups, Gym Leader Castle have been validated) but the following
visible imperfections remain.

**Priority order (user-set 2026-05-28):**
1. ~~Cursor / icon sprite corruption~~ — **FIXED 2026-05-28, user-confirmed.**
   Recomp `section_addresses` not reset on fragment clear; librecomp
   `unregister_runtime_fragment` (N64MR PR #2 `5a3579c` + PSR PR #5 `58d34c4`).
2. ~~Diagonal line across the screen at transitions~~ — **FIXED 2026-05-28,
   user-confirmed.** Conservative raster double-blended the shared diagonal of
   alpha-blended fullscreen quads; rt64 per-PSO conservative raster (`edcb06f`).
   **Closed with a noted caveat — see the entry below.**
3. POKéMON STADIUM panel bottom border missing — **DIAGNOSED 2026-05-28**
   (RT64 under-fills the bottom strip; unique oversized-tile/interior-
   sampling config). Fix not yet landed — see entry below.
4. Selected Game Pak card strip-overlay residual.
5. ~~GB Tower "start GB game" hang~~ - **FIXED 2026-06-02.**
   Red, Blue, and Yellow launch from GB Tower and reach gameplay via
   scripted TCP input. Save load/write is verified for MBC3 and MBC5 carts.
   See entry below.
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

- [x] **Visually corrupted "hand" pointer / cursor sprite** on
      certain menus. **FIXED 2026-05-28 (user-confirmed).** Root cause was
      a recompiler/runtime divergence, NOT corrupt texture/archive data:
      the recomp's `section_addresses[]` for a fragment section was not
      reset when the game cleared that fragment, so a fragment-id computed
      from a stale-relocated literal (`D_8D000000`) came out as −15, and
      `Memmap_SetFragmentMap(−15)` clobbered `gSegments[1]` (the cursor's
      texture bank → fragment-31 code). Fixed in librecomp
      `unregister_runtime_fragment` (reset `section_addresses` on
      `Memmap_ClearFragmentMemmap`, mirroring `Memmap_GetFragmentVaddr`'s
      NULL-fallback). N64MR PR #2 `5a3579c` + PSR PR #5 `58d34c4`. The
      session-2 "archive `unk_02` = 0xFFF1 decompression" theory was
      refuted by direct measurement (`unk_02` = 0). Full post-mortem in
      memory `project_cursor_corruption_2026_05_28.md`.
      *Original report (historical):* Re-confirmed 2026-05-28: on the Gym Leader Castle
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

- [ ] **POKéMON STADIUM panel bottom border missing (main menu).**
      **DIAGNOSED 2026-05-28 — root cause is an RT64 render bug (NOT
      texture data, NOT geometry, NOT authentic). FIX NOT YET LANDED.**

      *Symptom.* The central, highlighted "POKéMON STADIUM" icon panel
      renders with a thick blue frame border on its top and sides but
      **no blue border along the bottom** — the blue label band ends right
      at the "POKéMON STADIUM" text baseline and the gold selection bracket
      sits against bare wallpaper. (Originally mis-described in
      `NOTES_TO_CODEX.md` as "lowercase descenders clipped / gap too
      small"; the real artifact is the panel frame's missing bottom edge.
      User-confirmed it is the highlighted *panel's* missing bottom, not
      the selection bracket.)

      *How the panel is drawn.* The icon is an RGBA16 image (172 px wide,
      ~156 rows tall) split into horizontal strips, drawn as a 2-column Vtx
      grid (left col quads #0-7 @`0x8011D460`, right col #8-15). Sub-DL
      @`0x8011D860` issues, per strip: `G_SETTIMG` (image base set via
      segment `0x0F` = `0x804C6D80`) → `G_LOADTILE` → `G_VTX` → `G_TRI2`.
      Strips 0-6 each load/map 20 image rows (0..139); the bottom strip
      (quad #7/#15) maps image rows 140-152 (12 rows) over a 12-unit-tall
      quad at a clean 1:1.

      *Ruled out (each VERIFIED directly):*
      - **Texture data** — dumped the full RGBA16 image + alpha map; it has
        a complete, symmetric frame, and rows 140-152 (the bottom border)
        are fully OPAQUE navy. Not corrupt/missing.
      - **Geometry / t-coords** — dumped the grid; quad #7 maps rows
        140-152 over its height at clean 1:1, same scale as every strip
        above. No overrun, no squish.
      - **Draw issued** — decoded the sub-DL; all 16 strips incl. the
        bottom one emit SETTIMG/LOADTILE/VTX/TRI2.
      - **Texture load** — tmem ring shows the bottom strip loaded from the
        right source (img row 140) with the right rows.
      - **Scissor** — full-screen 640×480; nothing clips the panel.
      - **Render scale** — missing at native 1× *and* upscaled → not a
        high-res sampling artifact.
      - **Conservative rasterization** — forced OFF
        (`RT64_CONSERVATIVE_RASTER=0`): bottom STILL missing (and the HUD
        seam lines return). Exonerated.
      - **UV / sampling transparent texels** — ruled out by the stretch
        test: at both 12px and 96px the bottom-most pixel samples the SAME
        UV (row 152, opaque navy), yet 12px → background, 96px → navy. Same
        UV, different result.
      - **Highlight** — moved the highlight to BATTLE NOW; the missing
        bottom stayed on STADIUM. All other panels (BATTLE NOW, EVENT
        BATTLE, GALLERY, OPTIONS) render complete bottom borders.

      *Probable cause — RT64 tile/height-dependent under-fill.* The bottom
      strip's quad rasterizes at the correct screen position & height
      (vtx_ring: internal y[256,268], a 12px quad) but only its top ~3px
      get filled; the bottom ~9px show wallpaper. Live Vtx poke stretching
      the quad taller makes it fill completely → RT64 is under-filling the
      short strip.

      *What makes STADIUM strip #7 UNIQUE* (verified by decoding the other
      panels' grids): its `G_LOADTILE` loads 16 image rows (140-155,
      lrt=155) but the quad samples only 12 (rows 140-152) — the quad
      samples the **interior** of an oversized loaded tile, never reaching
      the tile's bottom edge. Every other strip (incl. GALLERY's
      equally-short 12-unit bottom strip, which renders FINE) has its quad
      t-range reach/exceed the loaded tile's bottom (clamping at the edge).
      This oversized-tile / interior-sampling configuration is the prime
      suspect.

      *Not yet pinned:* the exact RT64 code path. `RasterVS` is a clean
      transform and the framebuffer `drawColorRect` math
      (`rt64_rsp.cpp` ~L1073) computes correctly for the strip, so the
      under-fill is elsewhere — candidates: framebuffer tile/resolve, or
      rasterization of the wide-short (100×12) triangle pair when the tile
      is oversized. **Next step:** add an always-on RT64 instrumentation
      hook logging the bottom strip's submitted triangle + tile dims +
      final draw/resolve rect, then fix in `lib/rt64`. Fix at the RT64
      layer — NO game-side hack.

      *Confidence it is a divergence (not authentic):* the texture frame is
      symmetric (~12-13px borders top & bottom), so real hardware shows a
      ~12px navy bottom border; we show ~3px. (Could not obtain an
      original-hardware reference image through the available tooling.)

      *Diagnostic tools added this session (kept):* `tools/_gbi_decode.py`
      (F3DEX2 DL decoder), `_dump_rgba16_tex.py` (RGBA16 texture+alpha
      dumper), `_peek_bin.py`, `_capture_panel.py` (isolate a panel's quad
      stack from vtx_ring), `_find_panel_loads.py` (isolate a panel's
      RGBA16 tile loads from tmem ring), `_poke_quad7.py` / `_poke_height.py`
      / `_poke_flip_t.py` (live Vtx pokes for the stretch/flip/height
      experiments), `_panel_tris.py`.

- [x] **Diagonal line across the entire screen at transitions.**
      **FIXED 2026-05-28 (user-confirmed) — CLOSED WITH A CAVEAT (below).**
      (Reported + screenshotted 2026-05-28.) A thin straight corner-to-corner
      line over the whole frame during transitions/fades (and faintly over
      dark 3D scenes).

      *Root cause:* conservative rasterization, enabled unconditionally on
      RT64's N64 triangle PSO (`b60cf10`) to close fragment-57 HUD Vtx-grid
      gaps, also covers the **shared diagonal** of the game's fullscreen
      transition-fade quad (a translucent quad = 2 triangles). On an
      **alpha-blended** quad that diagonal is blended twice → visible seam.
      (Opaque double-coverage is invisible, so HUD grids were fine.)
      Confirmed via `RT64_CONSERVATIVE_RASTER=0`: the diagonal vanished but
      the HUD grid seams returned — same mechanism, opposite signs. (The
      fade is alpha-blended *triangle* geometry, not a `Rectangle`-projection
      rect — an initial rect-only gate missed it.)

      *Fix:* per-PSO conservative rasterization in lib/rt64 (`edcb06f`,
      shipped to rt64 `main`/`dev`). `RasterShaderUber` pipelines `[8]→[16]`
      with a conservative bit; the draw-call selector routes **rect OR
      alpha-blended** draws to the non-conservative variant, keeping
      conservative raster on opaque triangles (HUD grids stay closed).

      **⚠ CAVEAT (the closure is conditional on this):** the gate is BROAD —
      it disables conservative raster for *any* alpha-blended draw, which
      also routes 3D translucent geometry through the non-conservative
      pipeline. Verified on menus/transitions + HUD, but **NOT yet
      regression-tested in a live battle**. If translucent 3D surfaces show
      new hairline seams or a perf dip in battle, NARROW the gate to a
      fullscreen-coverage test (intended tool: an always-on draw-signature
      ring — proj type / alphaBlend / triangle count / coverage — queried
      across a transition window; do NOT screenshot-time the transient).
      `RT64_CONSERVATIVE_RASTER=0` remains a global force-off escape hatch.
      Memory: `project_diagonal_transition_line_2026_05_28.md`.

- [x] **GB Tower "start GB game" hang / launch path** (reported 2026-05-28;
      **FIXED 2026-06-02**). The embedded GB Tower now launches Red, Blue,
      and Yellow from Transfer Pak cart images, reaches live gameplay, and
      persists saves.

      **2026-06-02 current status.** Verified with scripted TCP input and
      screenshot captures:
      - Yellow/CGB: `build/gbtower_yellow_cgb_fix/yellow_cgb_after_fix.png`
        reaches the full-color Yellow title screen; `yellow_cgb_after_start.png`
        reaches the Continue/New Game/Option menu. Gameplay proof:
        `build/gbtower_yellow_gameplay_fix2/yellow_gameplay_after_continue.png`
        and `yellow_gameplay_start_menu.png`.
      - Blue/DMG: `build/gbtower_blue_lui_current/blue_lui_gameplay.png`
        and `blue_lui_start_menu.png` reach gameplay and the in-game menu.
      - Red/DMG: `build/gbtower_red_title_start_current/` reaches gameplay
        and the in-game menu; Red shares the same 0xA40 GB Tower ucode path
        as Blue.
      - Route reports contain no crash/error markers. Yellow uses boot
        `0x80147310`, size `0x9E4`; Blue uses boot `0x80147DB0`, size
        `0xA40`.

      **Verified navigation route from fresh boot.**
      1. Press `Start`, wait about 4.2s.
      2. Press `Start`, wait about 4.2s to the title screen.
      3. Press `A`, wait about 4.2s to Game Pak Check.
      4. Press `A`, wait about 4.2s to the main select screen.
      5. Press `A` on the default `POKEMON STADIUM`, wait about 4.2s to the
         Stadium map.
      6. Press `DRight`, wait about 1.5s to highlight the giant Game Boy /
         GB Tower.
      7. Press `A`, wait about 2.5s to the GB cart select screen.
      8. Cart select: Yellow is the default 1P cart; Red is `DRight` once;
         Blue is `DRight` twice.
      9. Press `A` to launch the selected cart.

      **Fix details.**
      - Transfer Pak input/config is generic and cart-driven. Runtime env
        overrides are `PSR_TRANSFER_PAK_P1_ROM`/`_SAVE`, `_P2_...`, and
        `_P3_...`; save files are flushed after dirty GB SRAM writes.
      - Pokemon Stadium submits GB Tower display/copy microcode as
        `M_AUDTASK`, so dispatching solely by task type incorrectly routes it
        to `aspMain`. The game project now checks the ucode boot signature
        before the `M_AUDTASK` fallback:
        `0xA40` -> `gbTowerMain` for DMG/Red/Blue, and `0x9E4` ->
        `gbTowerColorMain` for CGB/Yellow. This is game-side microcode
        registration, not a generic runtime hardcode.
      - `gbTowerColorMain` was generated from the ROM's 0x9E4 CGB GB Tower
        ucode. `gbTowerMain` was regenerated from the 0xA40 DMG GB Tower
        ucode.
      - RSPRecomp now emits unsigned-source shifts for RSP `lui`, `sll`,
        and `sllv` (`U32(...) << ...`) before sign-casting, avoiding signed
        left-shift UB in generated microcode. Both GB Tower RSP sources were
        regenerated after that fix.

      **Save validation.**
      - Yellow/MBC5 temp save:
        `build/save_tests/yellow-save-test-result.json`, 32768 bytes,
        SHA-256 changed from
        `053826A1F36338E9A73BF268E4B2362D36CE1441D2D8BDEA3ED309793FE026CF`
        to
        `7483FAAC75FC21FB6529583E79A43736DAA6019C2F51C394D38DE5EB03701295`.
        Captures: `build/gbtower_yellow_save_test/yellow_save_prompt_1.png`,
        `yellow_save_prompt_2.png`, and `yellow_after_save.png`.
      - Blue/MBC3 temp save:
        `build/save_tests/blue-ingame-save-test-result.json`, 32768 bytes,
        SHA-256 changed from
        `E96A8ACBDE5C01AAF7D37DD5E7D5FE7F8C456CB8B2194D903F891696F4B4E8CA`
        to
        `C99BEC49C2E66BCF93089BF12C85A7256E58C58F89EC1DADBE417FABDB1EEAE6`.
        Captures: `build/gbtower_blue_ingame_save_test/blue_gameplay_before_save.png`,
        `blue_save_prompt_1.png`, `blue_save_prompt_2.png`, and
        `blue_after_ingame_save.png`.

      **Historical repro (superseded):** booting a Game Boy game in the
      embedded GB Tower used to hang to a black screen.

      **Historical repro from 2026-05-28 (superseded):** load a GB cart
      image into P1 Transfer Pak —
      `transfer_pak.cfg` `p1_rom=pokemon-yellow.gbc` / `p1_save=...`, OR env
      `PSR_TRANSFER_PAK_P1_ROM`/`_SAVE` (the cfg is read from `current_path`
      = `build/`, so env is easiest from the repo root). Main menu →
      POKéMON STADIUM → Right (highlights "GB TOWER") → A → select the
      `YELLOW` cart → it loads, detects the cart, then **hangs (black
      screen, graphics pipeline frozen)**.

      **Cart layer works.** Transfer Pak serves Yellow correctly (log shows
      the Nintendo logo + `POKEMON YELLOW` title + type 0x1B read off the
      image; MBC5 banking via writes to tpak blocks 0x400/0x500/0x580). The
      GB Tower **detects** the cart ("YELLOW ID 20921"). The embedded
      emulator code (`gb_tower` seg, `func_8000A630` @ ROM 0xB230) is NOT
      stubbed and runs. The GB game ROMs are **not** built into Stadium
      (verified: only 3 GB-logo signatures in the 32 MB ROM = the
      emulator's boot/reference logo; `POKEMON YELLOW/RED/BLUE` strings ×2
      each = cart-detection tables) — the GB Tower is a generic cart-reading
      emulator, so a cart image is required (no physical cart needed).

      **Root cause — cooperative-scheduler deadlock at the GB Pak power-up
      delay.** Two threads (`dump_threads`):
      - Thread A is in `func_81200AA8` (frag-1 GB Tower init) → `osGbpakInit`
        (funcs_72.c) which creates queue `0x80102220`, calls `osSetTimer`
        for a ~0.2 s power-up delay (countdown `0x895440` ticks → mq
        `0x80102220`, msg `0x80102238`), then **blocks in `osRecvMesg`**
        waiting for that timer message. (`__osPfsGetStatus` earlier in
        `osGbpakInit` is shimmed and works.)
      - Thread B (`func_812033F4` @ frag-1, called by `func_81206D9C` ←
        resident `func_8000D678`/E1C0) **spins in a tight loop**
        (`L_81203A80: bnel $t4,$0; lbu $t4,0x5DC8($s0)`) waiting for a busy
        byte at `[*D_8122B2C0]+0x5DC8` to clear — a flag `osGbpakInit`
        clears only after its delay completes.

      The queue's `validCount=0` → the timer message was **never delivered**
      despite minutes elapsing. The tight spin loop has **no preemption
      point** (pure load+branch, no call), so it monopolizes ultramodern's
      cooperative N64-scheduler token: the timer thread's `osSendMesg`
      (timer.cpp:133) can't take effect and `osGbpakInit`'s (higher-pri)
      thread can't be scheduled to clear the busy flag → both stall forever
      → frozen graphics / black screen. On real N64 the timer is an
      **interrupt** and `osGbpakInit` is **preemptively** scheduled, so
      hardware never deadlocks. Same bug *family* as the prior softlocks
      fixed by voluntary preemption (free-battle-modal / petit-cup), but
      voluntary preemption isn't catching THIS loop (no yield check on the
      back-edge of a call-free spin).

      **Original fix-layer diagnosis = runtime / recompiler (N64ModernRuntime + maybe
      n64recomp).** Make a CPU-bound recomp thread yield so a higher-pri
      runnable thread and timer-message delivery aren't starved — e.g.
      insert a preemption/yield check on loop back-edges (recompiler), or
      have the host monitor force-preempt a thread that's spun N iterations
      without yielding (runtime). NO game-side hack. (Supersedes the old
      CLAUDE.md decision #4 "stub it" stance — user opted in to making the
      GB Tower actually boot.) `osGbpakInit` itself is correct N64 code;
      the gap is preemptive scheduling under the recomp.

      *Diag tools added:* `tools/_gbhdr.py`, `_scan_gb_roms.py`. Diagnosis
      via `dump_threads` (→ `build/last_error.log`), `trace_recent`,
      `get_last_pc_trail`, `rdram_peek` of the OSMesgQueue, and the
      `[transfer-pak]` stderr trace (`PSR_TRANSFER_PAK_DEBUG=1`).

      **2026-05-31/2026-06-01 navigation reference (verified by scripted
      TCP input + screenshots).** Current screenshot run:
      `build/gbtower_diag_red_rawwindows_20260601/`. Earlier reference:
      `build/gbtower_asset_wait_trace_screens5_20260531/`.

      Route from a fresh boot/intro state:
      1. Press `Start`, wait ~4.2s.
      2. Press `Start`, wait ~4.2s. Verified title screen:
         `02_title.png`.
      3. Press `A`, wait ~4.2s. Verified Game Pak Check:
         `03_gamepak_check.png`.
      4. Press `A`, wait ~4.2s. Verified main select:
         `04_main_select.png`.
      5. Press `A` on default `POKEMON STADIUM`, wait ~5s. Verified
         Stadium map: `05_stadium_overworld.png`.
      6. Press `DRight`, wait ~1.5s. Verified GB Tower highlighted:
         `06_gbtower_highlighted.png`.
      7. Press `A`, wait ~2.5s. Verified GB cart select:
         `07_cart_select.png`.
      8. Press `DRight`, wait ~1.0s. Verified Red/2P selected:
         `08_red_selected.png`.
      9. Press `A` to launch Red.

      Notes for future scripted runs: the first `Start` may only advance the
      boot/attract sequence; the second `Start` should land on the title
      before pressing `A`.

      2026-06-01 update: the original access violation and subsequent
      runtime function lookup misses are fixed. The fix chain was:
      normalize static `jr $ra` return links; seed static entries from
      relocation targets, indirect-dispatch fallthroughs, branch
      fallthroughs, link targets, and bounded raw fallthrough windows;
      fix orphan LO16/HI16 relocation pairing; and add the GB Tower custom
      RSP task path. Confirmed resolved misses included link/runtime
      targets `0x8120B04C`/`0x80128B9C`, `0x8120B6E8`/`0x80129238`,
      `0x8120B6F4`, and `0x8120A9DC`/`0x8012852C`.

      2026-06-02 note: the post-navigation black-screen blocker described
      in the 2026-06-01 update is superseded by the current fixed status
      above. The missing piece was the CGB GB Tower ucode path: Yellow uses
      a second boot blob (`0x9E4`) that must be selected before the generic
      `M_AUDTASK -> aspMain` fallback.

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
