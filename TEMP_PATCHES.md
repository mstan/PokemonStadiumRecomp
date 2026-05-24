# TEMP_PATCHES.md

Registry of active per-site / Stadium-specific workarounds in this
repo. Each row points at the smell so a future audit can find them
without archaeology. The intended workflow:

1. When a future change wants to "fix all of these properly", read
   this file as the punch list.
2. For each entry: revert the patch, reproduce the bug via the
   listed repro path, fix at the proper-fix layer, validate, then
   delete the entry from this file.
3. **Add new entries here** whenever a new per-site workaround
   ships in `extras.c` / `game.toml`. Mark every patch site with a
   matching `PSR_TEMP_PATCH: <id> — see TEMP_PATCHES.md` comment so
   `git grep PSR_TEMP_PATCH` cross-references this file.

Entries are in apply-order (oldest first); status reflects whether
the per-site patch is currently active.

---

## free-battle-modal-softlock (retired 2026-05-24)

| field | value |
|---|---|
| status | **retired** — superseded by the ultramodern voluntary-preemption mechanism (N64MR `b0364f2` host-monitor + yield flag; PSR `25d157c` wires `ultramodern_scheduler_tick` into `pkmnstadium_trace_entry`). `func_8000771C` is no longer in `[patches.ignored]`; the native recompiled body (the tight `while (func_80001C90() == 0) {}` loop) now resolves naturally — each call to `func_80001C90` inside the loop body hits `trace_entry`, and once the busy-waiter has held the CPU >200 ms without a context switch the host monitor flags a yield, swapping to the audio scheduler so it can drain `external_messages` and post the DONE that flips `D_800846C0.queue.validCount`. No game-side priority manipulation, no hand-written wrapper. |

## petit-cup-softlock (retired 2026-05-24)

| field | value |
|---|---|
| status | **retired** — superseded by the ultramodern voluntary-preemption mechanism (N64MR `b0364f2` host-monitor + yield flag; PSR `25d157c` wires `ultramodern_scheduler_tick` into `pkmnstadium_trace_entry`). The `func_80003680` JPEG-decoder inlined busy-wait (`while (func_80001C90() == 0) {}` at vram `0x80003770`/`0x80003778`) now drains naturally: when the busy-waiter has held the CPU >200 ms without a context switch, the host monitor flags a yield; the next `trace_entry` (from `func_80001C90`'s own re-entry inside the loop body) drains externals and swaps to the head of `running_queue`, letting the asset-loader / audio threads run and flip the predicate. No game-side priority manipulation needed. |

## fragment57-vtx-seams (retired 2026-05-24)

| field | value |
|---|---|
| status | **retired** — superseded by conservative rasterization on the N64 triangle PSO (`lib/rt64` `b60cf10` adds `RenderGraphicsPipelineDesc::conservativeRasterEnabled`, wired through the D3D12 backend; `RasterShader::createPipeline` opts in unconditionally). The `func_82D01758` hook + `pkmnstadium_patch_fragment57_ui_seams` + `pkmnstadium_overlap_vtx_*` helpers + `FRAG57_*` defines are all removed. With `D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON`, the rasterizer covers every pixel a triangle touches and the shared-edge tie-break that previously left dashed seams no longer applies. `RT64_CONSERVATIVE_RASTER=0` on the runner is a force-OFF escape hatch if a future workload regresses. Vulkan/Metal backends treat the flag as a no-op pending `VK_EXT_conservative_rasterization` / Metal plumbing — D3D12 is currently the only backend in regular use on this project. Verified 2026-05-24 against the seam-visible baseline: cards 2/3/4 and the WARNING banner render without seam dashes. The faint artifact remaining on card 1 is a different bug class (the still-active `fragment57-selected-card-overlay` replacement). |

## fragment57-selected-card-overlay

| field | value |
|---|---|
| status | **active** |
| applied | pre-2026-05-23 (inherited) |
| sites  | `extras.c::func_82D022E8` (replacement); `game.toml [patches].ignored = ["func_82D022E8", …]` |
| markers | `PSR_TEMP_PATCH: fragment57-selected-card-overlay` (TO ADD at both sites) |
| repro | Game Pak Check → cursor over a card with a real cartridge loaded → select it. Pre-patch: the native `func_82D022E8` emits a 5-strip RGBA overlay layer that should render as near-transparent decoration over the selected card's status text. On RT64, this overlay leaks stale/yellow TMEM rows across the card. The replacement omits the overlay entirely (card background draws before; real text draws after, both unaffected). |
| signal that the proper fix has shipped | Selected cards on Game Pak Check render the 5-strip overlay cleanly (no stale TMEM leakage) *with this entry reverted (un-ignore the function, delete `extras.c::func_82D022E8`)*. |
| memory note | none (consider adding one) |
| proper-fix layer | **RT64** (most likely) or **librecomp RDP cmd interpreter** — characterize what set-tile / load-tile / TEXRECT sequence the original 5-strip emit produces, find which step trips a stale-TMEM rendering. Possible alternative: SP precision in the s-coord delta for the narrow strips. Shares investigative neighborhood with `fragment57-vtx-seams` — both probably want one trip through RT64's UI-fastpath code. |

## force-expansion-ram (retired 2026-05-23)

| field | value |
|---|---|
| status | **retired** — moved from a Util_InitMainPools hook to `GameEntry::on_init_callback` in `src/main/main.cpp`. No more Stadium-specific helper in `extras.c` or hook in `game.toml`; the write happens once at game init in the runner, which is the right layer for "missing-from-HLE hardware-detect side-effect" runtime facts. The deeper "N64Recomp static-analysis pass to identify dead-code expansion-pak gates" is still a future investment that would benefit other titles, but isn't required for Stadium. |

## asset-pending-bypass (retired 2026-05-24)

| field | value |
|---|---|
| status | **retired** — superseded by the ultramodern voluntary-preemption mechanism (N64MR `b0364f2` host-monitor + yield flag; PSR `25d157c` wires `ultramodern_scheduler_tick` into `pkmnstadium_trace_entry`). The `func_800484E0` hook + `ctx->r2 = 0` bypass is removed from `game.toml`; the spin in `func_8000D2B4` now drains naturally because the host monitor forces a yield once the busy-waiter has held the CPU >200 ms without a context switch, giving async loaders a chance to flip the pending count. |

## audio-uaf-fragment36-voice-stop (retired 2026-05-23)

| field | value |
|---|---|
| status | **retired** — PSR `cefd9f6` shipped the generic mechanism: `lib/N64ModernRuntime/librecomp` `SecondaryVoiceTableLayout` (chain `[+0x90, +0x2C]` to wavetable-pointer-array base, silence `voice.unk_038`), registered from `src/main/main.cpp` at startup, fires from the existing `main_pool_pop_state` hook alongside the libnaudio-side helper — both range-check the freed `[saved.L, current.L)` on every pool pop. Per-scene `pkmnstadium_audio_stop_voices` + `func_8004FF20` hook deleted. Open follow-up (separate future work, does NOT retire any current hack): extend the chain language with an **indexed-deref step** (offset + index-source-offset + index-size + stride) so the full 4-level Stadium chain through `voice+0xC2` `v1_index*4` to individual wavetable pointers can be range-checked. The current 2-step chain reaches the array base, which is sufficient for Stadium's UAF source (array base and its wavetable entries both live in the freed 1 MiB SoundBank buffer) but not generally. |

## memmap-get-fragment-data-context (retired 2026-05-23)

| field | value |
|---|---|
| status | **retired** — orchestration logic (TLS-stack input save + 3-step resolve combining `recomp_resolve_synthetic_fragment` + `recomp_addr_in_loaded_variant` + `recomp_resolve_via_data_context`) promoted to librecomp as `librecomp_fragment_input_push` / `librecomp_fragment_resolve_exit`. Stadium's `game.toml` hooks now call those librecomp helpers directly; the per-game `pkmnstadium_memmap_get_enter` / `pkmnstadium_memmap_get_exit` were deleted from `extras.c`. Pattern is reusable by any game with variant-aware fragment buckets. See `lib/N64ModernRuntime/librecomp/src/overlays.cpp`. |

---

## How to revert one entry safely

1. `git grep "PSR_TEMP_PATCH: <id>"` to find every site (should be exactly the rows listed under `sites`).
2. Remove the lines / blocks at each site, including the `PSR_TEMP_PATCH:` marker comment.
3. Rebuild + run the listed repro. Confirm the original bug returns.
4. Now apply the proper-fix-layer change.
5. Re-run the repro. Confirm the bug is fixed *without* the per-site patch.
6. Delete the entry from this file.
7. Update the linked memory note: change "active" status to "superseded by …".

If reverting one entry breaks an apparently-unrelated entry's repro,
they're coupled — note the coupling in both rows before proceeding.
