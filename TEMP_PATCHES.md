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
