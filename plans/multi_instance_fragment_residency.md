# Multi-instance fragment residency — design (ChatGPT-conferred 2026-06-22)

Fixes #9 (and the whole stale-fragment-alias class). Load-bearing shared-engine change
(librecomp + eventually N64Recomp); benefits Zelda64Recomp, PMS-J, Stadium 1/2, etc.

## Confirmed root cause (measured)
The recomp tracks each fragment id/section as a SINGLE latest mapping:
- `get_function(addr)` = EXACT-match `func_map` (runtime addr -> recomp_func_t*). Each
  `load_overlay` registers funcs at the runtime slot (ram+off) AND the link-vram, and sets the
  GLOBAL `section_addresses[idx] = ram`.
- RELOC in generated C = `section_addresses[sec] + off` (one global per section).
When a fragment reloads/moves, the OLD runtime-address aliases are NOT removed (FUNC_MAP ALIAS
SCAN: nearly every superseded instance kept 2-2433 live entries) and `section_addresses[idx]`
holds only the latest base. So a computed call (e.g. `func_80019328: temp_v0 = func(0,0)`) into a
stale instance, or RELOC under the wrong base, returns stale data -> the geo source pointer lands
in the graph pool -> `process_geo_layout` walks garbage -> crash. Built graph nodes embed
pointers INTO the fragment, so a model's fragment must stay resident while its graph is alive; a
battle keeps multiple models' graphs alive => MULTIPLE live instances of one section concurrently.

## Decision: implement B now (no regen), evolve to A later
- **A (long-term, cleanest):** codegen `RELOC(sec,off) = ctx->active_section_base[sec] + off`,
  set per-context on dispatch. Requires N64Recomp codegen change + REGEN ALL GAMES.
- **B (ship first, no regen):** treat the GLOBAL `section_addresses[]` as **per-guest-thread
  execution context**. Correct under our COOPERATIVE scheduler (one guest OSThread runs at a
  time; runtime owns all context switches). Invariant: *at every moment a guest thread executes
  native code, `section_addresses[]` reflects that thread's current active instance bindings.*

## Dispatch redesign: range-based for fragments, exact for resident
Stop using one giant runtime-alias `func_map` as authoritative (that is why stale aliases
survived). Resolve `runtime addr -> live instance range -> canonical link entry -> func`.
Split maps:
```
unordered_map<uint32_t, CompiledEntry>   resident_func_map;     // exact runtime addr (static)
unordered_map<OverlayKey, CompiledEntry> overlay_link_func_map; // section+link-off (starts + interior aliases), STABLE
// live instances resolved by RANGE:
struct FragmentInstance { uint64_t instance_id, generation; uint32_t fragment_id, section_index,
    link_base, runtime_base, size; uint32_t runtime_end()const{return runtime_base+size;} bool alive; };
struct OverlayKey { uint32_t section_index, link_pc_or_offset; };
struct DispatchTarget { recomp_func_t* func; FragmentInstance* instance /*null=resident*/;
    uint32_t runtime_pc, link_pc, section_index, dispatch_entry_target; };
```
`get_function`/resolve: if addr in a live instance range -> link_pc = link_base+(addr-runtime_base)
-> overlay_link_func_map[{section,link_pc}] -> func, bound to that instance. Old runtime addresses
resolve ONLY if their live instance is still alive => stale aliases become impossible.

## Per-instance relocation base (B) — the hard part
`section_addresses[]` becomes logically per-thread. Machinery:
```
struct ThreadSectionContext { int32_t section_base[MAX_SECTIONS]; uint64_t section_gen[MAX_SECTIONS]; };
// context switch OUT: memcpy(cur_thread->section_base, section_addresses, ...)
// context switch IN:  memcpy(section_addresses, next_thread->section_base, ...)   (fragment slots only, for speed)
// dispatch enter/leave (DISPATCHER owns the binding lifetime, not generated funcs):
void enter_dispatch_target(dt){ if(!dt->instance)return; sec=dt->instance->section_index;
    push_section_base(sec, section_addresses[sec], active_section_generation[sec]);
    section_addresses[sec]=dt->instance->runtime_base; active_section_generation[sec]=dt->instance->generation; }
void leave_dispatch_target(dt){ if(!dt->instance)return; pop_section_base(dt->instance->section_index); }
// guest-thread-local stack:
struct SectionBaseFrame { uint32_t section_index; int32_t old_base; uint64_t old_gen; };
thread_local std::vector<SectionBaseFrame> section_base_stack;   // guest-thread-local, not host TLS
```
Dispatch loop owns binding across TAILCALLS (recomp_handle_tailcalls):
```
dispatch_loop(target){ while(target){ dt=resolve(target); bind(dt.instance); dt.func(rdram,ctx);
    unbind(dt.instance); target = ctx->next_tailcall; } }
```

## Critical hazards (B) — must handle ALL
- **Nested call into a DIFFERENT instance of the same section** -> the stack restores caller's base
  (without a stack, caller resumes with callee's base and silently corrupts relocations).
- **Tailcalls** -> the dispatcher (not individual funcs) owns base-binding lifetime.
- **Yield deep inside fragment code (BIGGEST hazard)** -> on EVERY cooperative exit the scheduler
  must save the thread's section bases, and restore on resume. Audit all exits: osRecvMesg BLOCK,
  osSendMesg BLOCK, osYieldThread, thread start/stop, sleep/timer waits, scheduler-forced yield,
  exception/trampoline paths, interp-fallback yielding. Miss one => another thread runs w/ wrong bases.
- **Resident code using RELOC(sec,off) after returning from a fragment** -> base must remain that
  instance's while the thread is logically still operating on that model. Rule: bind on dispatch
  into an instance; restore on return to the caller's previous context; thread context preserves
  whatever caller context was active. (Don't always pop-immediately-on-return if the thread is
  still logically on that instance.)
- **Direct generated calls that bypass dispatch (cross-section)** -> no binding occurs for the
  callee. If direct calls are only intra-section, fine. If cross-section, need codegen (A) OR
  generated call wrappers OR force cross-section calls through the dispatcher. KEY VERIFICATION GATE.

## Instance lifetime / eviction
Rule: an instance is valid while its guest RDRAM range remains present and not overwritten by
another payload. Do NOT invalidate just because the same id loaded elsewhere. Hooks:
- **main_pool_free/alloc (cleanest signal):** on free, mark instances fully inside the freed block
  dead; on alloc, invalidate any live instance overlapping the new allocation (owner != that inst).
- **PI-DMA / load_overlays write into a range:** invalidate overlapped instances unless this write
  IS the canonical load for that same instance.
- **Memmap_ClearFragmentMemmap(id):** marks the slot "no longer latest" — does NOT necessarily kill
  old absolute instances (graphs may still point at their bytes).

## Phasing (ship order)
- **Phase 0 — shadow + verify (no behavior change):** add the range resolver + report alongside the
  current dispatch. Gate: every crash/miss inside an old frag range resolves via the range resolver;
  every stale exact func_map entry is reported with its owning dead/alive instance.
- **Phase 1 — precise unregister / stop the bleeding (SHIP FIRST):** track each instance's registered
  runtime-alias keys; on instance death erase `func_map[key]` only if owner_instance_id matches.
  Safety net even after Phase 2. Gate: re-run the FUNC_MAP ALIAS SCAN -> superseded instances show
  funcmap=0; the #9 crash stops or changes.
- **Phase 2 — range-based dispatch:** replace authoritative runtime aliases with
  runtime->live-instance->canonical-link-entry. Gate: old graph ptr into a still-live instance
  resolves by range; ptr into the graph pool does NOT resolve; ptr into a freed/reused instance
  does NOT resolve.
- **Phase 3 — runtime-managed section-base binding (B):** enter/leave_dispatch_target push/pop +
  per-thread save/restore of section bases on all cooperative exits. Gate: a battle with multiple
  models keeps each instance's relocations correct; siblings (PMS-J, Stadium 2) non-regressed.
- **Long-term — A:** codegen `ctx->active_section_base[sec]` + regen all games (retire B).

## Verification across games (every phase)
Re-run the FUNC_MAP ALIAS SCAN + [geoseg] probe; reproduce the #9 switch-in crash (must stop);
spot-check PMS-J + Stadium 2 boot+play (no regressions); the separate party-select->cup lost-wakeup
(#5) is unrelated but watch it doesn't worsen (the geo probe's scheduler_tick perturbs timing).
