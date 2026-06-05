# Open issues — PokemonStadiumRecomp

Living list of known gaps. Use this as a pre-flight before starting
a new work session.

## Current playable build — issue list

The base game runs end-to-end (Quick Battle, Free Battle, Stadium
cups, Gym Leader Castle have been validated). **Still early
development.** As of 2026-06-05, #11 (the GB Tower launch crash) is
FIXED together with #7 (register-quit softlock) on a single build —
see the depth-bounded tailcall fix below.

**OPEN as of 2026-06-05 (milestone snapshot):**
- **#10 Occasional slight audio crackle (new).**

**ACTIVE priority among OPEN issues:** #10. The issue numbers are stable IDs
(referenced across this doc, commits, and memory), not the work order.

**Original priority order (user-set 2026-05-28, superseded above for open work):**
1. ~~Cursor / icon sprite corruption~~ — **FIXED 2026-05-28, user-confirmed.**
   Recomp `section_addresses` not reset on fragment clear; librecomp
   `unregister_runtime_fragment` (N64MR PR #2 `5a3579c` + PSR PR #5 `58d34c4`).
2. ~~Diagonal line across the screen at transitions~~ — **FIXED 2026-05-28,
   user-confirmed.** Conservative raster double-blended the shared diagonal of
   alpha-blended fullscreen quads; rt64 per-PSO conservative raster (`edcb06f`).
   **Closed with a noted caveat — see the entry below.**
3. ~~POKéMON STADIUM panel bottom border missing~~ — **FIXED 2026-06-04.**
   See entry below.
4. ~~Selected Game Pak / Player 1 card strip-overlay lines~~ — **FIXED
   2026-06-04.** See entry below.
5. ~~GB Tower "start GB game" hang~~ - **FIXED 2026-06-02.**
   Red, Blue, and Yellow launch from GB Tower and reach gameplay via
   scripted TCP input. Save load/write is verified for MBC3 and MBC5 carts.
   See entry below.
6. ~~Latent Stadium-side gfx pool UAF / race~~ — **FIXED 2026-06-04.**
7. ~~Register Pokémon (Transfer Pak cart import) quit → softlock~~ —
   **FIXED 2026-06-04.** See entry below.
8. ~~Game Boy games have no audio (GB Tower / Transfer Pak cart import)~~ —
   **FIXED 2026-06-04.** GB APU output is now queued from
   `osGbSetNextBuffer` into the host audio path. Red, Blue, and Yellow
   verified with steady queued audio and nonzero audible PCM. See entry below.
9. ~~Registered Pokémon do not persist (Transfer Pak cart import)~~ —
   **FIXED 2026-06-04.** Stadium now uses FlashRAM saving, and the runtime
   handles Stadium's low-level Flash command path. A freshly restarted runner
   re-enters Registration showing the saved registered set. See entry below.
11. GB Tower crashes on selecting any game — **FIXED 2026-06-05 (user-verified).**
    Root cause: the GB Tower emulator's fetch-decode-execute loop is an
    unbounded chain of tail calls with a CONSTANT guest SP. The #7 fix's
    "drain nested tailcalls locally" re-entered `recomp_handle_tailcalls` once
    per loop iteration, growing the host C stack without bound → host stack
    overflow (`0xC00000FD`) on cart launch. #7 and #11 share this exact dispatch
    path and present with identical SP/r31; the discriminator that separates
    them is nesting DEPTH (#7 finite/shallow <256; #11 unbounded, observed
    7680+). Fix (N64Recomp `25ecf9f` + N64ModernRuntime `78949d6`):
    `tailcall_dispatching` became a nested-trampoline depth counter; call
    wrappers drain locally below depth 1024 (preserves #7's finite menu-exit
    continuation) and bubble up to the single outer trampoline above it (caps
    #11, converting the loop to iteration — lossless because constant-SP
    tailcalls carry no per-level work). Verified: GB Tower Red launches to
    live gameplay AND Registration → Quit returns cleanly to the
    Battle/Rules/Registration row, on the same build. See entry below.

10. Occasional slight audio crackle — **OPEN (new 2026-06-04).** Intermittent
    faint crackle during play. Distinct from the music-rate click fixed
    2026-05-28 (that was constant ~4/s decimation; this is occasional). See
    entry below.

- [x] **GB Tower crashes on selecting any game.** **FIXED 2026-06-05
      (user-verified).** Selecting any cart (Red/Blue/Yellow) in GB Tower used
      to crash on launch. First observed 2026-06-04 after the #7 register-Quit
      softlock fix landed (N64Recomp `d97d8d8`, N64ModernRuntime `981662a`).

      *Root cause (measured via the always-on PSR_CFDIAG control-flow ring).*
      #11 and #7 share the SAME nested-tailcall dispatch path in the N64Recomp
      call wrapper / `recomp_handle_tailcalls`, and both arrive with IDENTICAL
      register state (matching SP and r31) — so no SP/r31 test can separate them:
      - The GB Tower emulator (fragment 2, funcs around `0x8120A6A0`–`0x8120BD50`)
        is a fetch-decode-execute loop built from tail calls, recursing with a
        CONSTANT guest SP (`0x8011B788`) at every level.
      - The #7 fix made nested tailcalls "drain locally" — re-enter
        `recomp_handle_tailcalls`. For a finite continuation (#7) that is correct
        and shallow. For the GB loop it nests a fresh host trampoline per
        iteration → the host C stack grows without bound → host stack overflow
        (`0xC00000FD`) after ~7680 levels, on cart launch.
      - The inverse (always bubble up to the outer trampoline) caps the GB loop
        but skips #7's finite continuation tail (`func_80029008`'s
        `main_pool_try_free`) → over-unwind → #7 softlock. Confirmed by direct
        A/B: drain-only fixed #7 but crashed #11; bubble-only fixed #11 but
        softlocked #7.

      *Discriminator = nesting DEPTH.* #7 is finite/shallow (<256 cumulative
      drain events); #11 is unbounded (7680+). A threshold cleanly separates them.

      *Fix (N64Recomp `25ecf9f` + N64ModernRuntime `78949d6`).*
      `ctx->tailcall_dispatching` is now a nested-trampoline DEPTH counter
      (`++`/`--` around the drain loop in `recomp_handle_tailcalls`, per guest
      thread). Generated call wrappers, on a pending NON-local tailcall not
      already resolved by the return-continuation / local-label handlers:
        - depth < 1024 → drain locally via `recomp_handle_tailcalls` (gives #7
          its known-good behavior — runs the frame's tail and resumes);
        - depth ≥ 1024 → bubble up so the SINGLE outermost trampoline iterates
          (gives #11 its known-good behavior — O(1) host stack). Bubbling
          mid-loop is lossless because constant-SP pure tailcalls carry no
          per-level work. 1024 is ~4× above #7's depth and ~7× below the
          observed overflow depth.

      *Verification (live build, debug-server driven + screenshots).*
      - #11: Game Pak Check → POKéMON STADIUM → STADIUM → GB Tower → Red → launch
        reaches LIVE Pokémon Red gameplay (battle scene); process stays alive,
        `send_dl`/`dp_complete` advance, 8506 GB Tower trace events, no overflow.
      - #7: Game Pak Check → … → POKé CUP → Poké Ball division → Registration →
        Quit returns cleanly to the Battle/Rules/Registration row; `send_dl`
        advanced across Quit; PSR_CFDIAG ring shows NO `THREAD-ENTRY RETURNED`
        / SP-mismatch and a zero nested-return counter.
      Both verified on the SAME build.

      *Owed (behavior-neutral, tracked):* the gated `recomp_cf_note`
      `call-bubble-dispatch` scaffolding in every wrapper (no-op unless
      PSR_CFDIAG=1), the `[jrdiag]` block + `set_source_files_properties` trace
      block; Yellow/Blue not yet harness-verified (fix is cart-agnostic — same
      GB interpreter loop). The depth threshold (1024) is empirically chosen;
      a legit finite continuation nesting >1024 would bubble — the cf-ring would
      flag it as an unexpected `call-bubble-dispatch`.

- [x] **Register Pokémon quit → softlock (Transfer Pak cart import).**
      **FIXED 2026-06-04.** The 2026-06-04 regression came from the
      local-tailcall continuation change in N64Recomp `755b13a`: it preserved
      local label continuations, but restored the old non-local nested-tailcall
      bubble-up path. On Registration → Quit, `PSR_CFDIAG=1` showed
      `call-bubble-dispatch` events followed by the known SP-mismatch cascade
      out of `Game_Thread`.

      N64Recomp `d97d8d8` keeps the local continuation handling, but non-local
      nested tailcalls now fall through to the reentrant
      `recomp_handle_tailcalls` drain instead of restoring the previous host
      return and returning upward. N64ModernRuntime `981662a` bumps the
      N64Recomp gitlink to that fix.

      Validated on the live build with the exact route:
      Game Pak Check → POKéMON STADIUM → STADIUM → POKé CUP → POKé BALL →
      Registration → Quit. Quit returned to the Battle/Rules/Registration row;
      `send_dl` advanced from 11492 to 11595 over the next two seconds,
      `dp_complete` advanced with it, and the fixed `PSR_CFDIAG` log contains
      no `THREAD-ENTRY RETURNED`, `call-bubble-dispatch`, or SP-mismatch event.

      The prior root-cause analysis and fix history are retained below.

      *Root cause (confirmed).* A nested call whose callee tail-jumped, while
      an outer tailcall dispatch loop was active, hit the call-wrapper
      bubble-up early-return and was FLATTENED into the outermost dispatch
      loop. So the whole Registration menu (fragment 61, reached from
      `func_80029008`'s `jalr $v0`) ran as one flat tailcall chain anchored at
      `func_80029008`'s continuation `0x80029028`. On menu exit the SP no
      longer matched `func_80029008`'s `recomp_call_sp`; the per-call
      `if (ctx->r29 != recomp_call_sp) return;` guard fired and the over-unwind
      cascaded out of `run_thread_function`, ending `Game_Thread`. Confirmed via
      the always-on PSR_CFDIAG control-flow ring (`build/cfdiag.stderr.log`:
      menu ran as a cyclic tailcall chain at hrt=`0x80029028`; `THREAD-ENTRY
      RETURNED entry=0x8002B330 r31=0x80029028`).

      *Fix.* (a) call wrappers no longer bubble up when an outer dispatcher is
      active — they fall through to `recomp_handle_tailcalls` so the
      continuation drains its OWN pending tailcall chain locally and resumes;
      (b) `recomp_handle_tailcalls` made reentrant (save/restore the
      `tailcall_dispatching` flag); (c) static-tail guards restored
      (`recomp_request_tailcall_func_target`; jump-to-own-return → normal
      return). The earlier "pool exhaustion" theory was REFUTED by measurement
      (kept below for the record).

      *Cleanup still owed (behavior-neutral; tracked, not yet done):* gated
      `recomp_cf_note` scaffolding emitted in every call wrapper (no-op unless
      PSR_CFDIAG=1); the `PKMNSTADIUM_TRACE_HOOKS` `set_source_files_properties`
      block in CMakeLists; the `[jrdiag]` block + `_n_` audio-skip filter in
      `extras.c`. The always-on PSR_CFDIAG CF ring in librecomp is doctrine-
      aligned and may stay. Diagnostic history retained below.

      *Repro (USER-CONFIRMED; driven via debug-server input + screenshots).*
      Game Pak Check →A→ Please Select (POKéMON STADIUM, A) →A(overworld STADIUM)→
      A(POKé CUP)→A(POKé BALL)→ POKéMON STADIUM screen (Battle|Rules|Registration)
      → DRight×2 to Registration, A → submenu (Register/Check/Delete/Quit) →
      DDown×3 to **Quit**, A → wedge. Screen freezes on the POKéMON STADIUM screen
      with the Battle/Rules/Registration buttons GONE (torn down mid-transition).
      Process ALIVE: `frame`/`vi`/`submit_audio` climb; `send_dl`/`dp_complete`
      frozen. Reliably reproducible.

      *Visible mechanism (measured).* The graphics frame builder `dp_intro`
      (thread `func_8000183C`, dp_intro.c:175) is a per-frame servant: it blocks
      on `osRecvMesg(&D_8008468C)` for a render REQUEST, builds+submits the gfx
      task, replies `'DONE'` to `D_800846A4`. At the wedge it is blocked on the
      request queue with **both `D_8008468C` and `D_800846A4` empty** — i.e. the
      PRODUCER stopped posting render requests. The producer is the main game
      state machine `Game_Thread` (29BA0.c:776), which drives frames via
      `func_80001BD4`/`func_80001BA8`. At the wedge **`Game_Thread` is GONE from
      the thread dump** (funcs_68 absent on all 8 live guest threads). Its OSThread
      `pThreads`@0x8007F730 exists but has no host thread and `queue=0` → its host
      thread RETURNED. But `Game_Thread` is an infinite `while(1)` dispatch loop
      with NO return path in C — so its termination is a recompiler control-flow
      divergence, not game logic.

      *Refuted hypotheses (all by direct measurement on the live wedged process):*
      1. **Pool exhaustion — NO.** `sMemPool`@0x800A6070 `available`=0x34F640
         (~3.45 MB free); the always-on `[pool] ALLOC FAIL` logger never fires;
         `D_800AA660/664`=NULL is just the GB-emulator IDLE state (in a clean
         repro `func_8000D738` never even runs — no GB threads). Pool is hardcoded
         `POOL_END_6MB=0x80600000` in BOTH paths → 6 MB is the cap on hardware
         too; "bump to 8 MB" was never valid.
      2. **Voluntary preemption — NO.** `PSR_DISABLE_VOLUNTARY_PREEMPTION=1` still
         wedges. (Tick CADENCE does shift WHERE it dies — more trace hooks → more
         `ultramodern_scheduler_tick` calls → earlier death — so it's timing-
         sensitive, but preemption is not the trigger.)
      3. **`func_84203E6C` (frag61 registration menu) return — NO.** jrdiag at its
         `TRACE_RETURN`: `ra=0x80029028 == host_return_target=0x80029028`,
         `tailcall_pending=0`, SP balanced. It returns cleanly to `func_80029008`.
      4. **`func_80029008`'s `jr $ra` section-reloc fixup (`section_addresses[5]`) —
         NO.** jrdiag shows `sec5=0x80000400` (un-relocated), so the fixup is
         skipped; `func_80029008` returns correctly many times during navigation.

      *Localization (where it DOES diverge).* Call chain at divergence (from a
      stack scan of `Game_Thread`'s stack 0x80080000–0x800818E0):
      `Game_Thread → func_80029BC0 (STATE_STADIUM_MENU) → func_80029984 (Stadium
      fragment loop) → func_80029008 (FRAGMENT_LOAD_AND_CALL, 29BA0.c:54) →
      fragment 61 (func_84203E6C)`. After Quit, `func_84203E6C` returns correctly,
      but **`func_80029008` never reaches its normal `TRACE_RETURN`** — it diverges
      in its post-fragment-call tail: `result = func(...); main_pool_try_free(func);
      return result;`, specifically in/after the **`main_pool_try_free(func)` call
      (0x80002620)** that frees the just-exited fragment-61 block (which runs the
      block's destructor func-pointer). The recomp control-flow machinery then
      unwinds `Game_Thread` out of its `while(1)` via either the per-call
      `if (ctx->r29 != recomp_call_sp) return;` SP-mismatch cascade or a
      `recomp_request_tailcall` to a bad target → thread ends → producer dies →
      dp_intro starves → `send_dl` frozen.

      *Next step (FIX phase).* Pin the exact divergence inside `main_pool_try_free`
      / the freed-block destructor dispatch / the indirect-fragment-call return
      (extend the jrdiag to `main_pool_try_free` (funcs_87) and the destructor it
      runs). The fix lives in **N64Recomp** codegen (the GB-Tower-era tailcall /
      continuation / SP-mismatch `jr` machinery: `recomp_request_tailcall`,
      `host_return_target`, `recomp_handle_tailcalls`) — load-bearing; DISCUSS
      before editing. Memory: `project_register_quit_softlock_2026_06_02.md`.
      Key addrs: `Game_Thread` OSThread `pThreads`@0x8007F730; scheduler
      `D_800A62E0`@0x800A62E0; dp_intro client `D_80083CA0`@0x80083CA0, request
      queue `D_8008468C`, reply `D_800846A4`. Funcs: `func_80029008` (funcs_44,
      diverges), `func_84203E6C` (funcs_107, returns OK), `main_pool_try_free`
      (funcs_87, 0x80002620, suspect), `func_8000183C` (dp_intro, funcs_137,
      starved). **TEMP cleanup owed (all on branch, uncommitted):** CMakeLists
      `set_source_files_properties` trace block (now funcs_68/75/97/44/107); the
      `[jrdiag]` block + the `_n_` audio-skip filter in `extras.c`
      (`pkmnstadium_trace_entry_common`/`_return_common`). Revert all after the fix.

- [x] **Registered Pokémon do not persist (Transfer Pak cart import).**
      **FIXED 2026-06-04.** Not a regression — first reachable only after the
      register-Quit softlock (#7) was fixed.

      *Root cause.* The runner declared Stadium as `Eep4k`, but the game uses
      1Mbit cart FlashRAM for registered-party data. Stadium also does not call
      the runtime's high-level `osFlash*` stubs here; it uses bundled low-level
      `osEPiWriteIo` / `osEPiStartDma` Flash command wrappers. The first runtime
      Flash command implementation wrote data, but reported an MX_C Flash ID,
      making Stadium choose a `0x40` read-address multiplier while the runtime
      programmed 128-byte pages. That made the commit appear successful in RAM,
      but the next boot read the registered-party banks back from the wrong
      offsets and counted zero sets.

      *Fix.* Set `game.save_type = recomp::SaveType::Flashram`; emulate the
      low-level Flash command register path in `librecomp/src/pi.cpp`
      (status/id/read-array/page-program/sector-erase/chip-erase); initialize
      new Flash saves to `0xFF`; and report an MX_B/D Flash ID so Stadium uses
      `0x80` page addressing matching the runtime's 128-byte page granularity.

      *Verification.* Registration submenu → Register Pokémon → 1P Yellow:
      selected a full six-Pokémon set and chose **OK to Register**. The Flash
      save changed from SHA1 `f8b52429...` to `1104b2fb...`, the game displayed
      "Registration complete", and a freshly restarted runner re-entered the
      same Registration source selector showing **Registered Pokémon 1 Set(s)**
      instead of `0 Set(s)`.

- [x] **Game Boy games have no audio (GB Tower & Transfer Pak cart import).**
      **FIXED 2026-06-04.** GB Tower now queues embedded Game Boy audio from
      `osGbSetNextBuffer` through `pkmnstadium_gbtower_queue_audio`, normalizing
      the guest audio buffer address to KSEG0 before calling
      `ultramodern::queue_audio_buffer`.

      *Root cause.* GB Tower video/input were active, but the embedded GB
      emulator's audio buffer was never handed to the host runtime. The normal
      Stadium N64 audio task path does not carry GB sound while GB Tower is
      running its display/copy microcode, so the recomp needed an explicit hook
      at the GB audio-buffer handoff.

      *Fix.* Add the `osGbSetNextBuffer` hook at `0x8120730C`, implement
      `src/main/gb_audio.cpp`, and keep `PSR_DISABLE_GBTOWER_AUDIO=1` as a
      diagnostic opt-out. A companion N64Recomp local-tailcall continuation fix
      prevents the GB interpreter path from recursively nesting host tailcall
      dispatch while launching/playing the carts (N64Recomp `755b13a`, pin
      bumped).

      *Verification.* Cleanup build, real-time audio (`PSR_TURBO=0`,
      `PSR_AUDIO_DEVICE=S/PDIF`, debug port 4471): Red, Yellow, and Blue all
      launch, remain alive, queue ~60 audio chunks/s with no decimation, and
      report nonzero audible PCM over 240 sampled chunks per cart.

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

- [x] **Faint horizontal residual streaks on the SELECTED Game Pak /
      Player 1 card's 5-strip overlay** - **FIXED 2026-06-04.**
      RT64 now forces manual RDP texture sampling for TMEM-backed draws
      instead of native GPU sampler addressing. Verified across five
      cold-start passes on Game Pak Check Player 1 and 30 repeated main-menu
      frames; the intermittent Event Battle/Battle Now/Stadium panel streaks
      did not recur.
      Prior diagnosis retained below.

      Historical diagnosis (pre-closure): Card 1 at Game Pak Check showed
      the controller-icon graphic, but the 5-strip RGBA decoration had faint
      horizontal yellow lines along strip boundaries. Conservative
      rasterization (which fixed the row-divider seam class) is
      neutral on this artifact — verified by A/B with
      `RT64_CONSERVATIVE_RASTER=0`. Likely a different bug class:
      TMEM row sampling at strip boundaries, narrow-strip s-coord
      precision, or texture-wrap-at-edge. Resolved by the RT64 bilinear
      default above.

- [x] **POKéMON STADIUM panel bottom border missing (main menu).**
      **FIXED 2026-06-04.** The selected STADIUM card's source texture,
      geometry, and TMEM load were correct. The missing lower frame came from
      RT64 selecting a GPU framebuffer tile copy for the bottom strip even
      when the requested tile was outside the framebuffer's recorded last
      GPU-written rect. RT64 `e5d42d4` makes
      `FramebufferManager::makeFramebufferTile` reject uncovered framebuffer
      tiles, so these stale/partial matches fall back to the normal RDRAM/TMEM
      texture path. Verified with five fresh boots:
      the selected STADIUM lower edge is present, Player 1 strip rows remain
      below the artifact threshold, and a 30-crop Event Battle sample shows no
      intermittent yellow line artifacts.

      The prior diagnostic notes are retained below as historical context.

      Historical diagnosis (pre-closure):

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

- [x] **Latent Stadium-side gfx pool UAF / race** (newly named
      2026-05-23; reverified fixed/stale 2026-06-04).
      **FIXED 2026-06-04.** The submit-time GDL diagnostic was reading
      command words in the wrong byte order, producing a false malformed
      top-level target. After correcting the scanner to match RT64's
      `DisplayList` word order, a no-input attract run crossed the old
      failing range (`send_dl=3717`, `dp_complete=3717`) with no renderer
      stall. `build/attract_uaf_fixedscan_report.json` shows 1,024 recent
      submit targets and 1,024 recent walk targets, zero malformed heads,
      zero hits for the old bad targets (`0x80208F18`, `0x80208F20`,
      `0x80294B60`), and 955 matched submit/walk pairs that were
      byte-identical valid DLs.

      *Historical diagnosis before the 2026-06-04 recheck:* The attract-demo
      softlock root-cause was Stadium emitting a `G_DL CALL` into
      memory at `0x80208F18` that holds compressed audio/image
      bytes instead of DL commands. The RT64 fix prevents the
      renderer from softlocking on garbage data, but Stadium's
      pool/allocator is still emitting CALLs to wrong addresses.
      Likely the same family as the prior audio UAF (where freed
      wavetables get repurposed). Symptom may be invisible if no
      one walks the garbage area as a DL anymore, but a future
      bug could surface from the same broken pool management.
      At the time, proper investigation was thought to require the Ares
      oracle bridge to diff RDRAM at the moment Stadium emitted the bad
      CALL.

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

### Historical gate: Ares oracle bridge

Before the 2026-06-04 closeout, the remaining visible bugs each had
multiple plausible root-cause layers (recompiler emit, librecomp shim,
RT64 renderer, pret disasm correctness, or genuine game-side UAF that
real N64 hardware also has). Static-analysis triage narrowed but did
not always eliminate competing hypotheses.

The deterministic decider is the **Ares oracle workflow**
documented in [`DEBUG.md`](DEBUG.md): run Ares against the
original ROM and our recomp build to the same frame, byte-diff
RDRAM, find the first divergence, then trace backward to whether
the responsible write came from recompiled code (→ check pret's
claim about that address), from a librecomp shim (→ our libultra
emu bug), or from RT64 (→ renderer bug). The bridge is implemented
(see `n64recomp/ares-bridge/`) but currently has an in-process
blocker; the recommended next step was the **out-of-process
oracle** variant. At the time, the open bugs were diagnosis-blocked
at the "which layer" question and symptom patches were being used
while the oracle path matured.

## Historical roadmap / backlog

The unchecked tasks below are retained project roadmap and
architectural-cleanup notes. They are not open items in the current
playable-build issue list above.

### Scaffolding (historical phase)

- [ ] Replace placeholder SHA in `n64recomp.pin` with the real
      40-char HEAD of `../N64Recomp` after `setup.sh` runs. Currently
      pinned to a placeholder padded with hex.
- [ ] Verify `setup.sh` and `setup.bat` end-to-end on a clean clone.
- [ ] First `disasm/make init` + `disasm/make` run — confirm pret
      builds identically against `disasm/baseroms/us/baserom.z64`.

### Recompilation pipeline (historical phase)

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

### Runtime (historical phase; depends on N64ModernRuntime)

- [ ] Vendor or submodule N64ModernRuntime (the runtime that
      Zelda64Recomp / MM:R consume).
- [ ] Wire the runner CMake target — link `generated/*.c` against
      runtime + SDL2.
- [ ] First boot — expect to crash or blackscreen; record the
      first divergence.

### GB Tower (historical first pass)

- [ ] Enumerate all GB-related entry points reachable from
      resident `text`. Symbols of interest: `gb_tower`, `gb_mbc`,
      addresses near `0xB230`, `0xE1C0`, `0xE570`.
- [ ] Add `[[patches.stubs]]` entries in `game.toml` for each.
- [ ] Confirm main game (battles, minigames) reachable without
      hitting any stub.

### Oracle / divergence checking

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

### Modding (deferred — see proposals)

Explicitly **not** prioritized until the base game boots and is
playable.

- [ ] Mod manifest schema.
- [ ] RecompModTool / OfflineModRecomp integration.
- [ ] RecompModMerger.
- [ ] LiveRecomp / sljit JIT — almost certainly skipped.

### Documentation

- [ ] `MODDING.md` — flesh out post-MVP. Currently a placeholder.
- [ ] `docs/` — design notes on overlay handling, GB Tower
      stubbing, oracle architecture.
- [ ] Per-overlay README skeletons under `overlays/<name>/` once
      extraction tool runs.

### Audio

- [ ] **Occasional slight audio crackle (#10).** **OPEN (new 2026-06-04
      milestone).** Intermittent faint crackle/crackling during play. Distinct
      from the music-rate periodic click fixed 2026-05-28 — that was a constant
      ~4/s artifact from `skip_factor` decimation when the SDL queue exceeded
      100 ms, and it is gone (AI_LEN_REG pacing restored). This new report is
      *occasional*, not constant. Not yet root-caused; the always-on audio
      rings (`audio_queue_recent` / `audio_pcm_recent`, readers in `tools/`)
      are the place to start — query the queue-depth and post-resample HF
      windows around a crackle to see whether decimation is firing again
      (transient queue spikes) or it is upstream synthesis. No fix attempted in
      this milestone pass.

- [x] **Audio output keeps going to a "random" device; does not follow
      the Windows default. FIXED 2026-06-04.** `select_audio_device()`
      (`src/main/main.cpp`) used to pick the **first non-controller device in
      SDL's enumeration order** and pin it by name. That index is not the
      system default (on this box index 0 = "Headphones (Oculus Virtual Audio
      Device)") and the order shifts when devices are plugged/unplugged → the
      output endpoint wandered launch-to-launch and never tracked the user's
      chosen default. The original logic existed only to dodge a DualSense
      speaker hijacking the default, but it overcorrected into ignoring the
      default entirely.

      *Fix.* Rewrote selection to: (1) honor `PSR_AUDIO_DEVICE=<substring>`
      as an explicit pin; (2) otherwise query `SDL_GetDefaultAudioInfo` for the
      real system-default endpoint — if it is NOT a controller speaker, open
      the SDL default (`nullptr`), which on the forced WASAPI backend follows
      the Windows default endpoint **and live-migrates when the user changes
      it**; (3) fall back to the first non-controller named device only if the
      default itself is a controller. GOTCHA: `SDL_GetDefaultAudioInfo` rejects
      a null `spec` pointer ("Parameter 'spec' is invalid") — must pass a real
      `SDL_AudioSpec`. SDL here is FetchContent **2.30.3**, not rt64's bundled
      2.26. Verified live: log reports default `Headphones (SK010 Stereo)` →
      "SDL default (follows Windows default endpoint, live-migrates)" and boots
      to the main menu clean. (Output is still muted by default via
      `PSR_VOLUME`; set `PSR_VOLUME=1.0` to hear it — separate from routing.)
      Memory: `project_audio_initiative_2026_06_02.md`.

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
