# COSIM — Per-VI-Frame Differential Co-Simulation (Tier 0)

Groundwork + execution playbook for a differential co-simulation harness that
grades the PMS-USA recomp against measured truth and lets us knock out
divergences — including "invisible" ones (state that is wrong but not yet
visible on screen/audio).

Read `../recomp-template/DIFFERENTIAL-COSIMULATION.md` (agnostic decision
procedure) and `../recomp-template/N64/DIFFERENTIAL-COSIM-PROPOSAL.md` (the N64
feasibility analysis) first. This doc is the concrete build plan against the
current `main` code, with exact anchors.

---

## 0. Guiding principle (non-negotiable)

**Model the cycle clock from day one.** N64Recomp is HLE: RCP tasks and OS
calls complete in "zero guest cycles," which is *why* PMS has lost-wakeup
crashes, mispaced audio, and narrow game support (see the AI_LEN_REG pacing fix
already in-tree — "no timing model → over-production" was fixed by *modeling*
the timing). We do NOT need cycle-*exactness* to fix ordering/lost-wakeup races
— we need a **modeled guest-cycle clock**: an estimate of what each operation
*would* have cost on hardware, used to schedule when its completion event fires.

That modeled clock is simultaneously:
1. the **alignment axis** the co-sim needs (a deterministic, guest-execution-
   derived counter — unlike the current wall-clock VI, which is host-derived),
2. the **determinism source** (kills the wall-clock nondeterminism), and
3. the **bug fix** for the ordering/lost-wakeup class.

So a `cycle_count` field is reserved in the state surface and protocol NOW, even
though at Tier 0 it starts as an approximate retired-instruction count. As
subsystems pivot HLE→LLE later, the same axis tightens — no re-architecture.

**Oracle = Ares** (2026 N64 accuracy leader: full LLE RSP+RDP, ~99%
n64-systemtest, hardware-validated timing). Bridge already half-built at
`n64recomp/ares-bridge/` (oracle TCP server + core glue + RSP ring exist;
`ares.lib` not yet built). CEN64 reserved only as an RTL tiebreaker if we later
go cycle-exact. Do NOT switch oracles.

---

## 1. Build arrangement (already set up)

Cosim work lives on `cosim/tier0` branches in the canonical checkouts, wired:

| Repo (branch) | Path | Role |
|---|---|---|
| PokemonStadiumRecomp `cosim/tier0` | `F:\Projects\n64recomp\PokemonStadiumRecomp` | harness entry, debug_server protocol, build flag |
| N64ModernRuntime `cosim/tier0` | `F:\Projects\n64recomp\N64ModernRuntime` | state snapshot/hash, deterministic VI, modeled clock, scheduler |
| N64Recomp `main` | junction `n64recomp` → `N64Recomp-issues` | recomp.h state surface (READ only for Tier 0) |
| rt64 `main` | junction `lib/rt64` → `rt64-cleanroom` | no Tier-0 edits |

PSR junction `lib/N64ModernRuntime` → canonical `N64ModernRuntime` (re-pointed
from `-cleanroom`). N64Recomp + rt64 need no Tier-0 edits.

**Build (THROTTLED — must not starve the machine):** cap parallelism and run
below-normal priority. Use `build_cosim.bat` (to be added; wraps
`build_ssanne.bat` with `PSR_BUILD_JOBS=6` and
`start "" /belownormal /b /wait`). Invoke via the REAL cmd.exe:
`& "$env:SystemRoot\System32\cmd.exe" /c "...\build_cosim.bat"`. First build
also needs `generated/` regenerated (already present in canonical PSR; a fresh
worktree would need `N64Recomp.exe game.toml`, ~145s). See
`memory/reference_buildable_worktree_recipe`.

---

## 2. State surface (what the hash covers)

Anchors: `N64Recomp-issues/include/recomp.h` `struct recomp_context` (507–593),
`fpr` union (491–502), `get/set_cop1_cs` host-fenv rounding (354–396);
`N64ModernRuntime/librecomp/src/sp.cpp` `RDRAM_SIZE = 8MB` (69);
`recomp.h` MEM_* macros + `rdram` host base (180–198).

**HASH FULLY (both-sides state) — never trim by hypothesis:**
- Integer: `r0..r31` (32×u64), `hi`, `lo`.
- CP0: `cop0_regs[32]`, `status_reg`, `mips3_float_mode`.
- CP1: `f0..f31` (the `fpr` union — hash `u64` to catch either lane).
- RDRAM: the real **8 MB** (`RDRAM_SIZE`), NOT the 1 GB mapped window.
- Reserved: `cycle_count` (modeled clock, §5) — hashed once real.

**EXCLUDE (host-only — would cause false divergences):** `f_odd` (host ptr),
`tailcall_target/pending/dispatching/func`, `host_return_target`,
`dispatch_entry_target/rejected/rejected_target`, ultramodern
thread/coroutine handles, RT64/audio-sink state, PC (currency mismatch — report
but don't cross-hash; see agnostic doc PC caveat).

**Documented blind spots (record, don't silently drop):**
- FP rounding mode lives in the **host** fenv (`get/set_cop1_cs`), not fully in
  `status_reg` — a rounding divergence is invisible to a status_reg-only hash.
- RSP full state / RDP pipeline / RCP timing: oracle-only (HLE has no model).
  These are *why* cycle-exact is infeasible on the HLE fork; they are the LLE
  pivot candidates later.

Serialize little-endian, arrays in fixed order.

---

## 3. Hash module  (N64ModernRuntime, new: `librecomp/src/cosim_state.cpp`)

- Per-subsystem **sub-hashes**: `cpu_int`, `cp0`, `cp1`, `rdram` — so a halt
  localizes WHICH subsystem split.
- **Chain hash**: cumulative across checkpoints (order-sensitive).
- **Incremental RDRAM page-hash**: 8 MB / 4 KB = 2048 pages; re-hash only dirty
  pages per checkpoint (cheap at 60 fps). Use `thirdparty/xxHash` (already a dep).
  Dirty-tracking: start with a full re-hash (correctness first); optimize to a
  write-watch/dirty-page bitmap only after profiling shows it dominates.
- Pure reads only (no guest-state mutation — some MMIO reads toggle bits).

---

## 4. Frame-quiescence checkpoint + determinism  (N64ModernRuntime)

Anchors: `ultramodern/src/events.cpp` `vi_thread_func` (213–296), wall-clock
advance (`total_vis` from `sleep_until(next)`, 225/236), retrace post (269–282,
coalescible `post_rcp_event`); `librecomp/src/vi.cpp` `wait_one_frame` (71–76,
spins on `total_vis`); scheduler `ultramodern/src/scheduler_tick.cpp`
`monitor_func` (87–104), tick entries (231–237); `scheduling.cpp`
`swap_to_thread` (10–23, priority-ordered = already deterministic);
`PokemonStadiumRecomp/src/main/main.cpp` `PSR_DISABLE_VOLUNTARY_PREEMPTION`
(1549–1560).

**Deterministic-VI mode (`N64_COSIM`):** post the VI retrace when the guest is
**quiescent** (all guest threads blocked awaiting the VI queue), NOT off the
wall clock. The checkpoint fires at that quiescent boundary → the frame
boundary becomes a pure function of guest execution.
- Add quiescence detection to the scheduler: a frame is "done" when
  `check_running_queue` is empty AND every guest thread is blocked on the VI
  retrace queue.
- Replace the `events.cpp` wall-clock `sleep_until` path (under `N64_COSIM`)
  with "advance `total_vis` on quiescence."

**Pinned scheduler:** force `PSR_DISABLE_VOLUNTARY_PREEMPTION` in cosim mode
(monitor thread off) and rely on the already-deterministic priority-ordered
`swap_to_thread`. Verify no host-timing back-doors remain (Gate 1 flushes them).

---

## 5. Modeled-cycle clock  (N64ModernRuntime) — the §0 principle, made real

- **Retired-instruction counter** on the recompiled CPU: approximate cycles via
  a per-instruction cost (start: 1 cycle/instr; refine with a cost table).
  Exposed as `cycle_count` in the state surface + protocol.
- **Modeled task latencies:** where HLE completes an RSP/AI/DP task "instantly"
  (`osSpTaskStartGo` interception, AI queue, DP), post its completion event on
  the modeled clock at `now + modeled_cost(task)` instead of immediately. Even a
  rough causal model (cost > 0, completion observable only after the CPU could
  see the start) fixes most lost-wakeups.
- **Calibration loop (once Ares is live):** Ares measures the *real* per-task
  cycle cost; feed those numbers into `modeled_cost(...)` so the HLE timing
  converges toward hardware without full LLE. Only where a calibrated model
  still can't reproduce guest-visible state do we LLE that subsystem.

Tier 0 ships the counter + a first-cut `modeled_cost` and reserves the axis;
calibration is a later tier gated by the Ares bridge.

---

## 6. TCP protocol  (PokemonStadiumRecomp, extend `src/main/debug_server.cpp`)

Anchors: `handle_command` dispatch (404–415), param parsers `get_int/get_uint`
(354–402), `recomp_runtime_get_rdram()` + `MEM_W` guest access (422–425),
existing `set_button`/`pc_trail`/`fast_forward`. Reuse this scaffold; add:

| Command | Action |
|---|---|
| `cosim_stride {n}` | set checkpoint stride (VI frames); fix at launch for lockstep |
| `cosim_step {frames}` | run N checkpoints, park at quiescence, reply `{cp, vis, cycle, chain}` |
| `cosim_chain` | current cumulative chain hash + cp + vis + cycle |
| `cosim_sub` | per-subsystem sub-hashes (`cpu_int/cp0/cp1/rdram`) |
| `cosim_regs` | full CPU/CP0/CP1 field dump (for field-diff) |
| `cosim_window {n}` | last N checkpoint rows (bounded reporting) |
| `cosim_inject {field,val}` | Gate-3 fault injection |
| `cosim_reset` | reset incremental hash state |

All behind `#ifdef N64_COSIM`. The coordinator (`tools/n64_cosim.py`, later)
launches recomp (attract, no input) + Ares, steps both one frame, compares
`chain`; on mismatch dumps `sub` diff + `cosim_regs` + `cosim_window` + a
bisected first-divergent RDRAM address.

---

## 7. Build flag  (PokemonStadiumRecomp `CMakeLists.txt`)

Anchor: `add_executable` (153–183), `WITH_ARES_BRIDGE` pattern (305–307).
```cmake
option(N64_COSIM "Per-VI-frame differential co-simulation harness" OFF)
if(N64_COSIM)
    target_compile_definitions(PokemonStadiumRecomp PRIVATE N64_COSIM=1)
    target_sources(PokemonStadiumRecomp PRIVATE src/main/cosim_harness.cpp)
endif()
```
Runtime-side (`cosim_state.cpp`, deterministic-VI, modeled clock) guarded by the
same define, propagated to the N64ModernRuntime target. Zero effect when OFF.

---

## 8. Validation gates — trust NOTHING until these pass (agnostic doc §gates)

1. **A-vs-A determinism = 0** across the PMS attract run (no input). THE Tier-0
   forcing function: two recomp runs must be bit-identical at every frame
   boundary. Initial failures expected: wall-clock VI, any host-timing leak,
   scheduler nondeterminism → fix each (§4). Nothing downstream is trustworthy
   until this is green.
2. **Injected fault halts at the right place** (`cosim_inject`) — the ONLY gate
   that catches a silently-blind coordinator. Never skip.
3. **Hash-vs-byte audit** every N frames (full 8 MB memcmp even when hashes
   match) — proves incremental page-hash maintenance.
4. (later) oracle-vs-oracle = 0 (Ares determinism) before any A-vs-B result.

Attract fixture: PMS title/attract is game-state-driven (no scripted input) →
identical empty input by construction. A concrete first-divergence candidate
already exists: the auto-attract `miniUpdateCamera`/`miniEkansInitCam` crash
noted in `game.toml`.

---

## 9. Execution task list (ordered; each independently verifiable)

Grunt-work-friendly for a long-running executor. Do IN ORDER; each ends in a
committed, measured checkpoint.

- **T0. Buildable-set sanity.** Throttled build of PSR `cosim/tier0` against
  N64MR `cosim/tier0` (N64_COSIM=OFF) → confirm the re-pointed junction still
  builds + boots to launcher. Baseline. *(may hand to Codex)*
- **T1. State surface + hash module.** `cosim_state.cpp` (§2/§3): sub-hashes +
  chain + full-RDRAM hash (correctness before dirty-page optimization). Unit:
  hash a known context/RDRAM → stable value; mutate one field → hash changes.
- **T2. Protocol.** Extend `debug_server.cpp` (§6): `cosim_chain/sub/regs`
  first (read-only), then `cosim_step/stride/window/reset/inject`. `N64_COSIM`
  build flag (§7).
- **T3. Frame-quiescence checkpoint.** Hook the checkpoint at the quiescent VI
  boundary (§4). `cosim_step` parks there. No determinism fix yet — just a
  stable stepping point.
- **T4. Gate 1 (determinism).** Run A-vs-A over attract. Expect failure; drive
  in deterministic-VI + pinned scheduler (§4) until bit-identical. **The
  milestone.** Land the deterministic-VI mode.
- **T5. Modeled-cycle clock.** Retired-instruction counter + first-cut
  `modeled_cost` task latencies (§5); expose `cycle_count`. Re-run Gate 1.
- **T6. Gates 2–3.** `cosim_inject` fault → halts at right cp/subsystem;
  hash-vs-byte audit.
- **T7. Ares oracle live.** Build `ares.lib` (`-DWITH_ARES_BRIDGE=ON`, prebuild
  Ares per `ares-bridge/DESIGN.md`); coordinator `tools/n64_cosim.py`; run the
  coarse per-VI-frame diff vs Ares. Grade the "only audio is off" hypothesis.
- **T8+. Knock out divergences** (incl. invisible ones): bracket → field-diff →
  name → faithful fix → re-run. Calibrate `modeled_cost` from Ares; pivot a
  subsystem HLE→LLE only where a calibrated model still can't reproduce
  guest-visible state.

---

## 10. Status

- [x] Repos on latest main; stale branches pruned; WIP parked.
- [x] `cosim/tier0` branches + buildable junction wiring. NOTE: canonical
      N64MR's internal `N64Recomp` junction must point at `N64Recomp-issues`
      (main) — it carries `include/recompiler/tcc_recompiler.h`, which
      `overlays.cpp` needs. A junction to the old park branch fails the build.
- [x] Integration points mapped (this doc).
- [x] **T0 — buildable-set sanity: PASS.** Throttled build (`build_cosim.bat`,
      N64_COSIM=ON) succeeds with 0 errors; boots + renders the 3D attract demo.
      Fixed `build_cosim.bat` exit-code detection (the start/cmd wrapper reports
      a bogus code on success; it now reads success from the ninja log).
      2026-07-02: helper also pins clang-cl exception mode (`/EHsc`) and
      `CMAKE_AR=llvm-lib.exe`, and freshens stale CMake compiler metadata when
      a previous configure captured devkitPro `ar.exe`; no-op helper rerun
      returns `build OK`.
- [x] **T1 — state hash module: DONE + PROVEN.** `cosim_state.{hpp,cpp}` in
      N64ModernRuntime/librecomp, wired into `librecomp/CMakeLists.txt`, compiles
      in the real target; standalone self-test (`-DCOSIM_STATE_SELFTEST`) 12/12.
- [x] **T2a — protocol layer: DONE.** `N64_COSIM` CMake option, PSR
      `cosim_harness.cpp`, debug-server commands `cosim_chain/sub/regs`,
      `cosim_stride/window/reset/inject`, and runtime active-context publishing
      are wired and build. Throttled cosim build passes; pre-game TCP smoke
      verified `ping`, honest pre-context `cosim_chain` error, and T3-gated
      `cosim_step` response.
- [x] **T3a — VI-quiescence detector + checkpoint capture: DONE.**
      N64ModernRuntime now exposes a cosim quiescence predicate over the
      scheduler's per-thread block table and VI event queue; the PSR harness
      captures checkpoint rows when that predicate fires. `cosim_step` now waits
      for those rows with a bounded timeout and reports live quiescence counters
      on failure. Throttled cosim build passes; pre-game TCP smoke verified
      `cosim_quiescence` and timeout reporting. This is still
      observed/unparked; deterministic VI delivery is next.
- [x] **T3b — deterministic VI drive + park/release: DONE.**
      `N64_COSIM` now routes game-started VI through a co-sim token gate instead
      of wall clock; `cosim_step` can issue deterministic VI tokens while waiting
      for the first parked checkpoint, and a parked checkpoint release advances
      exactly one direct VI edge. The host voluntary-preemption monitor defaults
      off in cosim builds. The quiescence predicate now classifies live
      `OSThread` state, allows the priority-0 idle thread plus blocked service
      mailboxes, and still requires a VI waiter, empty running queue, no runnable
      guest work, and no external pending events. `cosim_step` publishes a
      polled checkpoint when the deterministic VI gate is already quiescent, so
      it no longer depends on a later queue insert to notice an idle boundary.
      Throttled cosim build passes; protocol-only autoboot smoke shows two
      successful parked steps (`cp=2/vis=1`, then `cp=3/vis=2`) and
      `cosim_window` retains the checkpoint chain.
- [x] **T4 - Gate 1 A-vs-A determinism: PASS (60 VI, repeated).**
      `tools/n64_cosim.py gate1 --frames 60` passes twice on fresh paired
      launches with identical checkpoint chains through `cp=61/vis=60`.
      Determinism fixes landed in the cosim path: coordinated `cosim_start`,
      pre-game deterministic-VI gating, two-phase stable poll checkpoints,
      scheduler mutation/handoff epochs in quiescence, modeled RCP event queue
      ordering, synchronous cosim gfx submission before modeled DP completion,
      deterministic `osGetTime/osGetCount` read costs, and narrow normalization
      for host-only scheduler pointers plus inactive stack redzone bytes below
      each parked thread's saved SP.
- [x] **T5 - modeled-cycle clock slice: PASS.**
      `N64_COSIM` now reaches `RecompiledFuncs` so the already-generated
      `TRACE_ENTRY/TRACE_RETURN` calls feed a cosim-only modeled CPU counter
      without enabling the old diagnostic trace ring. The runtime exposes
      `cycle_count` as the protocol alias for the chain clock and `cpu_retired`
      as the current first-cut retired-instruction estimate; loop-backedge hooks
      also charge CPU cycles for future regenerated code. The existing modeled
      RCP task latencies remain the event-ordering clock. Throttled cosim build
      passed after the one-time generated rebuild; 5-frame smoke and 60-frame
      Gate 1 stayed deterministic, final 60-frame row:
      `cp=61/vis=60/cycle_count=55922092/cpu_retired=4449368`.
- [x] **T6 - gates 2-3: PASS.**
      `tools/n64_cosim.py gate1 --frames 60 --audit-every 10` completed six
      full 8 MB checkpoint-RDRAM byte audits with no differences. New
      `gate3` injected `rdram_byte` at `0x807FF000` in side A after frame 3 and
      detected the first mismatch at frame 4, localized to subsystem `rdram`
      and expected byte lane `paddr=0x7FFFF3`.
- [ ] **T7+** (Ares oracle, then divergence knockout) -
      IN PROGRESS. Ares v147 (`f533120df`) is built externally and the
      standalone `ares_oracle_server.exe` reports `ares-bridge: real`. Server
      commands now expose `read_cpu_state`, `read_hi_lo`, and bounded RDRAM
      digest/peek reads in recomp host byte order. `tools/n64_cosim.py
      ares-smoke --frames 2` passes (`pc=0xa40011f8`, then `0xa400136c`);
      `ares-gate --frames 20 --rdram-every 1` passes with full 8 MB
      Ares-vs-Ares RDRAM digest comparison every frame.
      Current post-revert gates: `gate1 --frames 60 --audit-every 15` passes
      through `cp=61/vis=60`, and `gate3` catches the injected RDRAM fault at
      frame 6.

      First live `oracle --frames 1` result: expected FAIL, not blind-pass.
      With 13 Ares warmup frames, frame 1 reports `subdiff=cpu_int,rdram`;
      recomp checkpoint is `cp=2/vis=1/cycle_count=1025054`, Ares PC is
      `0x80000078`, and the first RDRAM mismatch is `paddr=0x00000001`
      (`recomp` low RDRAM zeros vs Ares IPL-loaded image bytes
      `00 b0 0b 3c ...`). The oracle report also does a secondary diagnostic
      scan after the IPL span without suppressing the primary failure; current
      follow-up mismatch is `paddr=0x00100000`, again recomp zero vs Ares
      initialized RAM image data. A direct attempt to seed the 0x100000-byte IPL image
      into live recomp RDRAM was reverted because it corrupted boot-time
      libultra/runtime state and made Gate 1 time out before `cp=1`; this
      mismatch must be handled at the oracle surface or by a faithful runtime
      model, not by mutating live RDRAM after HLE boot state exists.
      Follow-up: the old PSR-only `gExpansionRAMStart=1` init patch is removed.
      Ares leaves that BSS global zero through the sampled boot window, and
      `disasm/src/util.c` confirms the expansion-RAM selector has no game-side
      writer. A throttled build plus `gate1 --frames 60 --audit-every 15` passed
      without the force (`cp=61/vis=60`). The oracle's diagnostic scan from
      `0x400` now passes the former `0x00068B90` mismatch and reaches
      `paddr=0x00077C97` (single-byte `01` vs `00` inside an otherwise matching
      pointer table), while the primary IPL low-RDRAM mismatch at `0x00000001`
      remains.
