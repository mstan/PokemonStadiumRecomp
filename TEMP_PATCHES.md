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

## free-battle-modal-softlock

| field | value |
|---|---|
| status | **active** |
| applied | 2026-05-08 (PSR commit `062cece`) |
| sites  | `extras.c::func_8000771C`; `game.toml [patches].ignored = ["func_8000771C", …]` |
| markers | `PSR_TEMP_PATCH: free-battle-modal-softlock` (both sites) |
| repro | Title → Free Battle → 1P → Rule → Cup → confirm any cup. Pre-patch: hangs at the cup-confirm screen, never advances. |
| signal that the proper fix has shipped | Free Battle Rule/Cup confirm advances cleanly *with both this entry and `petit-cup-softlock` reverted*. |
| memory note | `project_free_battle_modal_softlock_2026_05_08.md` |
| proper-fix layer | N64ModernRuntime/ultramodern: voluntary preemption of busy-waiting game threads. Stage 1 (per-call same-fn detector in `pkmnstadium_trace_entry`) was tried at 2026-05-09 and reverted (commits `5e91259`/`fe0d2fd` engine-fork; `646ba43`/this-revert PSR) — the per-call overhead in `trace_entry` perturbed audio-synth timing enough to flip a pre-existing audio UAF from rare to ~deterministic. The viable shape is a host-monitor "no context-switch in N seconds" flag set by ultramodern + a single atomic-load + branch in `trace_entry`; not yet implemented. |

## petit-cup-softlock

| field | value |
|---|---|
| status | **active** |
| applied | 2026-05-09 |
| sites  | `extras.c::pkmnstadium_petit_cup_yield`; `game.toml [[patches.hook]] func = "func_80003680" before_vram = 0x80003778` |
| markers | `PSR_TEMP_PATCH: petit-cup-softlock` (both sites) |
| repro | Title → Free Battle → 1P → Rule → Cup → confirm **Petit Cup** specifically (other cups don't always trip this site). Pre-patch: hangs at the cup-confirm screen with `Game_Thread` spinning at `funcs_60.c:1008` inside `func_80003680`'s inlined `while (func_80001C90() == 0) {}` loop. |
| signal that the proper fix has shipped | Petit Cup advances past cup-confirm cleanly *with both this entry and `free-battle-modal-softlock` reverted*. |
| memory note | `project_petit_cup_softlock_optionB_2026_05_09.md` |
| proper-fix layer | Same as `free-battle-modal-softlock` — both go away in one move when ultramodern grows voluntary-preemption-on-stuck-thread. |

## fragment57-vtx-seams

| field | value |
|---|---|
| status | **active** |
| applied | pre-2026-05-23 (inherited; FRAG57_SEAM_OVERLAP bumped 1→2 on 2026-05-23) |
| sites  | `extras.c::pkmnstadium_patch_fragment57_ui_seams` + `pkmnstadium_overlap_vtx_tile` + `pkmnstadium_overlap_vtx_grid_*` + `#define FRAG57_SEAM_OVERLAP`; `game.toml [[patches.hook]] func = "func_82D01758" before_vram = 0x82D01758` |
| markers | `PSR_TEMP_PATCH: fragment57-vtx-seams` (TO ADD at both sites) |
| repro | Boot → past title → Game Pak Check. Pre-patch: RT64 exposes 1-pixel yellow seam dashes between adjacent Vtx quads in fragment 57's UI panels (controller-card grids, warning header, info panels). Same Vtx grids drive the Main Menu icons (Gallery, Battle Now, central STADIUM panel) via different MTX, so seams appear there too. |
| signal that the proper fix has shipped | Game Pak Check cards and Main Menu icon panels render without row/column seam dashes at all render scales (including high-res) *with this hook reverted AND `FRAG57_SEAM_OVERLAP=0`*. |
| memory note | `project_fragment57_shared_grids.md` |
| proper-fix layer | **RT64** — shared-edge / sub-pixel handling for adjacent N64 Vtx quads at high render scale. Suspect: RT64 rasterizes at native resolution while the original geometry was hand-fitted for 320x240, so adjacent quads sharing a pixel boundary leave a sub-pixel gap. An edge-clamp or tile-merge pass in RT64 would obsolete every per-grid overlap workaround in this and other recompiled N64 games. Likely shares root cause with the unresolved **POKéMON STADIUM panel bottom clip** in ISSUES.md. |

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

## force-expansion-ram

| field | value |
|---|---|
| status | **active** |
| applied | pre-2026-05-23 (inherited) |
| sites  | `extras.c::pkmnstadium_force_expansion_ram`; `game.toml [[patches.hook]] func = "Util_InitMainPools" before_vram = 0x80002F58` |
| markers | `PSR_TEMP_PATCH: force-expansion-ram` (TO ADD at both sites) |
| repro | Boot without this patch. `Util_InitMainPools` reads `gExpansionRAMStart` (0 at boot, no writer anywhere in the binary) and takes the `POOL_END_4MB` path. The title-screen pokemon-models loader then hits an alloc failure because Stadium's working set on our runtime marginally exceeds the 3 MiB usable cap. With this patch, `gExpansionRAMStart=1` is forced into RDRAM at vaddr `0x80068B90` before the first read, activating the otherwise-dead `POOL_END_6MB` path (5 MiB usable). |
| signal that the proper fix has shipped | Title screen loads cleanly *with this entry reverted*, on a runtime that reports `osMemSize > 0x600000`. |
| memory note | none |
| proper-fix layer | **N64ModernRuntime / librecomp**: a generic "claim expansion pak present" mechanism that maintains the global-equivalent for any game where the symbol exists. **Plus N64Recomp static-analysis pass**: identify globals that gate code paths AND have no writer in the recompiled binary AND would be set by silicon hardware-detect on real hardware; auto-patch at runtime init. Pattern likely affects multiple N64 titles that ship dead-code expansion-pak paths. Stadium-specific only by symbol name; the underlying mechanism is generic. |

## asset-pending-bypass

| field | value |
|---|---|
| status | **active — undocumented, audit candidate** |
| applied | pre-2026-05-23 (inherited; no commit comment) |
| sites  | `game.toml [[patches.hook]] func = "func_800484E0" before_vram = 0x8004856C  text = "ctx->r2 = 0;"` |
| markers | `PSR_TEMP_PATCH: asset-pending-bypass` (TO ADD) |
| repro | UNKNOWN — needs identification. Reverting this hook should expose a wait-loop hang at `func_8000D2B4`, per the TOML comment. Need to find the user-facing path that hits it. |
| signal that the proper fix has shipped | TBD when proper-fix layer identified. |
| memory note | none |
| proper-fix layer | UNKNOWN — needs root-cause investigation before classification. Hypotheses: (a) another cooperative-scheduler busy-wait family (would fold into the `voluntary preemption` work that retires the softlock entries); (b) the pending counter is incremented on an async DMA path that completes too quickly on the recomp side, so the decrement happens before the increment from the caller's perspective. Investigate before the next layer-fix sweep — the one-line `ctx->r2 = 0` is too coarse for the long term. |

## audio-uaf-fragment36-voice-stop

| field | value |
|---|---|
| status | **active — load-bearing, full proper-fix attempt rolled back 2026-05-23** |
| applied | pre-2026-05-23 (re-enabled 2026-04-29 after prior disable; see TOML comment chain) |
| sites  | `extras.c::pkmnstadium_audio_stop_voices`; `game.toml [[patches.hook]] func = "func_8004FF20" before_vram = 0x8004FF20` |
| markers | `PSR_TEMP_PATCH: audio-uaf-fragment36-voice-stop` (TO ADD) |
| repro | Fragment36 cleanup path on certain transitions (originally: post-title audio UAF 2026-05-06; later: AREA_SELECT). Pre-patch: synth dereferences freed memory after fragment36 unloads its SoundBank. |
| signal that the proper fix has shipped | The generic mechanism covers BOTH the libnaudio voice list (already done) AND Stadium-side voice arrays. Audit step: revert THIS entry only, run AREA_SELECT and post-title transitions, confirm no UAF. |
| memory note | TODO |
| proper-fix layer | **Extend `librecomp::audio_uaf_protect`** (user's own code — `Copyright 2026 Matthew Stanley`, mstan fork; `lib/N64ModernRuntime/librecomp/src/audio_uaf_protect.cpp`). Add a `SecondaryVoiceTableLayout` registration that describes array-style voice tables with a wavetable-pointer chain to range-check (vs the existing intrusive-ALLink-list walker). Register Stadium's layout from `src/main/main.cpp` at startup. **Lessons from the 2026-05-23 attempt** (rolled back; left as design notes for the next try): (1) The wavetable-pointer-ARRAY base lives at voice.unk_090 + **0x2C**, not 0x28. The first attempt used 0x28 and silenced legitimate voices on every pool pop (audio went totally silent). (2) A 2-step static-offset chain reaching the array base is INSUFFICIENT — the actual wavetable pointer requires a 4-level chain through `v1_index*4` stored at `voice+0xC2`. Reaching it needs an **indexed-deref chain step** extension to `SecondaryVoiceTableLayout` (offset + index-source-offset + index-size + stride). (3) Even with the correct array-base chain, removing the per-scene hook entirely reintroduces issues — the per-scene unconditional silence catches voices whose wavetable pointer is in the freed range but whose array base is NOT, a gap the conservative array-base check misses. Proper fix sequence: (a) add indexed-deref step support, (b) register the full 4-level Stadium chain ending at the wavetable pointer, (c) verify the per-scene hook can be removed without regression, (d) delete the per-scene hook and function. **Estimated effort: 1-2 focused sessions** once the chain language extension is designed. |

## memmap-get-fragment-data-context

| field | value |
|---|---|
| status | **active** |
| applied | pre-2026-05-23 (inherited) |
| sites  | `extras.c::pkmnstadium_memmap_get_enter` + `pkmnstadium_memmap_get_exit`; `game.toml [[patches.hook]] func = "Memmap_GetFragmentVaddr"` entry + exit |
| markers | `PSR_TEMP_PATCH: memmap-get-fragment-data-context` (TO ADD at all four sites) |
| repro | Pattern-bucket fragment vaddr lookups (any input in `0x8FF00000` range). Without this hook, Stadium's single-pointer-per-id table `gFragments[id]` is ambiguous when multiple variants are concurrently host-resident, so lookups silently return whichever variant was registered most recently, producing geo-layout walks into wrong data. Crash signature: `process_geo_layout` walking an uninitialized buffer that came out of an ambiguous pattern-bucket fragment lookup. |
| signal that the proper fix has shipped | All pattern-bucket fragment lookups resolve correctly *with this hook reverted* AND the underlying disambiguation mechanism active in N64Recomp/librecomp. |
| memory note | none |
| proper-fix layer | **N64Recomp helper API** — promote the data-context-driven resolution pattern to a library facility. Most of the logic is already generic: `recomp_resolve_via_data_context`, `recomp_addr_in_loaded_variant`, `recomp_resolve_synthetic_fragment` all live in N64Recomp. The Stadium-specific surface is just the bridge from `Memmap_GetFragmentVaddr`'s symbol to those APIs; exposing a one-line wrapper would let extras.c-equivalents in other games invoke the same logic. The bridge itself is small enough that staying in `extras.c` is also defensible — choice is between "ship as N64Recomp idiom" vs "leave as documented per-game bridge." |

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
