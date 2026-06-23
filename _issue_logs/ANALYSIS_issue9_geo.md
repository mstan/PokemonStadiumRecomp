# Issue #9 — "Choosing Starmie crashes" — REAL trigger = Pokémon SWITCH-IN

## Reproduction notes (user)
- NOT on first send-out; crashes when a Pokémon is **switched in** mid-battle.
- Intermittent (heap-layout dependent) — same action sometimes doesn't crash.
- "Could be crashing when ANY pokemon is switched out/in", not Starmie-specific.

## Crash signature (build/last_error.log, enhanced pointer-window dump)
Indirect call to garbage. ra=0x80018C08 → call site 0x80018C00 inside
`process_geo_layout` (0x80018B70). The dispatch:
```
process_geo_layout: GeoLayoutJumpTable[gGeoLayoutCommand[0]]()
  t6 = gGeoLayoutCommand[0] = 0x80      (opcode byte, OUT OF RANGE: table has 40 entries)
  t8 = 0x8006F2B0 + 0x80*4 = 0x8006F4B0
  t9 = *t8 = 0xFCFFFFFF (garbage past table) -> jalr -> CRASH
```
Across loop iterations the bad targets were 0x8046F950, 0x804EF0E0,
0x80553100 (a VALID overlay addr, section 211 @ runtime 0x8052B0F0), 0xFCFFFFFF.

## Root mechanism
- `gGeoLayoutCommand` (0x800ABE00) = **0x80585224**, which is INSIDE the graph-node
  pool (`gGraphNodePool`) — the nodes the script itself builds (0x805xxxxx).
- The byte read as opcode (0x80) is the high byte of the pointer 0x80585220 stored
  at 0x80585224. The real node at 0x80585220 has a valid type byte (0x0E); neighbours
  0x8058524C (0x13), 0x80585270 (0x00) are valid too.
- So **the geo script pointer walked into the graph-node heap it was building** and is
  reading node pointers as opcodes. Section 211 (the model's geo fragment) ends at
  0x805542C0; 0x80585224 is past it, in the pool.

## Decompiled ground truth (disasm/src/geo_layout.c, src/util.c)
- `process_geo_layout(pool, segptr)`: `gGeoLayoutCommand = Util_ConvertAddrToVirtAddr(segptr)`,
  then `while (cmd) GeoLayoutJumpTable[cmd[0]]()`. No bounds check on cmd[0].
- `geo_layout_cmd_jump/branch/branch_and_link`: `gGeoLayoutCommand =
  Util_ConvertAddrToVirtAddr(cur_geo_cmd_ptr(0x04))` — read a target ptr and convert.
- `Util_ConvertAddrToVirtAddr` (src/util.c): phys→virt; segmented(<0x10000000)→
  `Memmap_GetSegmentVaddr`; fragment(0x81000000..0x90000000)→`Memmap_GetFragmentVaddr`;
  already-virtual→pass-through. **Memmap_GetSegmentVaddr=0x80001D10,
  Memmap_GetFragmentVaddr=0x800020E0 — recompiled GAME funcs reading game seg/frag tables.**
- CMD_SIZE_SHIFT = sizeof(void*)>>3 = 0 on N64 (decomp portability artifact, identity here).

## Leading hypotheses (need runtime trace / targeted instrument to pin)
1. A geo `jump`/`branch` target resolved WRONG via Util_ConvertAddrToVirtAddr
   (segment or FRAGMENT relocation) → gGeoLayoutCommand into the pool. Fragment/segment
   relocation is the recurring recomp-divergence class (cf. cursor-corruption stale
   section base). PRIME suspect.
2. Script ran off its end (missing/!=NULL terminator) into adjacent pool memory.
3. A command handler under/over-advanced gGeoLayoutCommand (off-by-4: 0x80585224 =
   node 0x80585220 + 4) — possible recomp codegen length bug in one handler.

## Next decisive data
- Function trace ring at crash: which geo_layout_cmd_* ran LAST (jump/branch ⇒ hyp 1;
  open/close/normal ⇒ hyp 2/3). Needs PSR_TRACE_HOOKS build (blocked by clang OOM on
  huge funcs at /O2 — fix: compile generated funcs at /O1 under trace).
- OR instrument Util_ConvertAddrToVirtAddr / Memmap_Get*Vaddr to log conversions whose
  result lands in gGraphNodePool while processing geo (these are recompiled game funcs).

## Relation to #16
Different mechanism (geo jump-table vs widget vtable) but same 3D-model display
subsystem. Likely independent root causes; both are "model subsystem fed wrong data".

## CONFIRMED ROOT CAUSE (2026-06-22, geo-state instrument)
Crash path: func_80019328 (src/19840.c) -> process_geo_layout(arg0, temp_v0->unk_08[i]).
Generated: 0x80019394 jal process_geo_layout, ret 0x8001939C; delay slot a1 = unk_08[i].

func_80019328 / func_80019420 DESTRUCTIVELY overwrite the geo SOURCE pointer with the
RESULT graph node:
    temp_v0->unk_08[i] = process_geo_layout(arg0, temp_v0->unk_08[i]);   // 19840.c:206/216
    arg1->unk_04       = process_geo_layout(arg0, arg1->unk_04);          // 19840.c:223
Safe ONLY if temp_v0 = arg1(0,0) returns FRESH source each use (model geo fragment
reloaded+re-relocated). On RE-USE with STALE/resident fragment data, unk_08[i] still holds
the prior graph-node ptr (0x80585xxx) -> process_geo_layout walks graph nodes as geo script
-> node-ptr high byte 0x80 read as opcode -> GeoLayoutJumpTable[0x80] OOB -> garbage jalr.

Geo-state dump evidence (build/last_error.log.* / r2):
- gGeoLayoutCommand=0x80585224 (graph pool), cmd[0]=0x80, stackIndex=2 (base).
- gSegments: only 0(identity),1,3 set; 2 and 4..15 NULL.
- gFragments non-null: 0(0x80262400),4(0x80114C10),51(0x80123360),239(0x80553100/0x136F0).
- geoCmd window = graph nodes (fn ptr 0x80553100 in frag239, DLs 0x8055F740/0x8055F988,
  links 0x80585220/0x8058524C, segref 0x0E110000 with seg14 NULL).
- Intermittent + on SWITCH-IN (re-process), NOT first send-out. User: "any pokemon switched in".

## RECOMP DIVERGENCE (the fix target)
Model geo-source fragment kept resident / not refreshed between uses, so arg1(0,0) returns
stale (already-overwritten) data. Hardware reloads+re-relocates the overlay each use ->
fresh unk_08[]. NEXT: identify the fragment + its load path (who calls func_80019328 with
which Fragment arg1), confirm whether recomp caches it, fix the reload/reset.

## DECISIVE: frag-reload ring (2026-06-22, register_runtime_fragment probe)
Symbolized backtrace: unhandled_lookup_trampoline <- process_geo_layout <- func_80019328
<- func_80019484 <- func_80019600 (model server thread). Confirms path.

FRAG (RE)LOAD RING (register_runtime_fragment = Memmap_RelocateFragment hook):
- id=239 is the MODEL fragment slot: reg_count=37, loaded at MANY addresses across the run
  (0x801D7A30, 0x801FE0B0, 0x80224140, 0x8046F950, 0x804EF0E0, 0x8052B0F0, 0x8053B0F0,
   0x804E7050, ... current gFragments[239]=0x80553100). So frag 239 IS reloaded fresh +
  moves around the heap (pool churn). NOT a stale recomp code-cache; it re-registers.
- The crash's bad call targets 0x8046F950 / 0x804EF0E0 are PREVIOUS frag-239 base addrs;
  0x80553100 is the CURRENT one. So the geo walk is CALLING stale frag-239 instance addrs.

REFINED ROOT CAUSE: the model's built GRAPH NODES (in gGraphNodePool, e.g. 0x80585xxx)
embed pointers (fn ptrs / DL ptrs / next-geo ptrs) relocated against the frag-239 instance
they were built from. The frag-239 SLOT is then reused for another model at a DIFFERENT
address, so those embedded pointers DANGLE. gFragments[239] holds only the latest instance,
so Memmap_GetFragmentVaddr cannot recover the old one. Walking/processing the surviving
graph after frag 239 moved -> stale pointer -> OOB geo dispatch / call to stale frag base
-> crash. Overlay INSTANCE-LIFETIME / use-after-reload class. Intermittent = depends on
whether the slot was reused/moved before the graph is next walked (heap/timing dependent).

OPEN QUESTION for fix layer: on HW the model's fragment presumably stays resident (or the
graph is self-contained) while its graph is alive; the recomp's pool reuse moves frag 239
under a live graph. Fix candidates: keep a fragment instance resident while any graph built
from it is alive (refcount), OR ensure graph build copies/owns its data, OR match HW pool
lifetime so the slot isn't reused under a live graph. CONFERRING with ChatGPT.

## ChatGPT refined verdict (2026-06-22, after frag-reload ring): MULTI-INSTANCE RESIDENCY
NOT pool determinism, NOT geo reset, NOT deep-copy. The recomp keeps only the LATEST
instance per fragment id (gFragments[id] / section_addresses[sec] / func_map all track the
newest load). When frag 239 reloads at a new address, the mapping for OLDER instances is
lost — but live graph nodes hold ABSOLUTE pointers into older frag-239 instances. Walking/
rendering those graphs -> recomp can't dispatch the old absolute address -> lookup miss ->
crash. On HW, an absolute ptr to an old instance stays valid as long as its RDRAM bytes are
present; re-registering the id at a new address must NOT invalidate prior-instance pointers.

HARDWARE-ACCURATE RULE: a relocated fragment instance stays executable/addressable as long
as its guest RDRAM bytes remain present; re-registering the same id at a new base only
changes the latest slot pointer, it does not invalidate prior instances.

FIX = multi-instance fragment residency + address-range dispatch:
- Registry of FragmentInstance {id, section, link_base, runtime_base, size, generation, valid}
  keyed by RUNTIME ADDRESS RANGE.
- register_runtime_fragment ADDS a new instance; does NOT delete older same-id mappings on
  gFragments[id] change. Invalidate an old instance ONLY when its RDRAM range is freed/
  overwritten/reused (overlap).
- Native lookup resolves by RUNTIME PC: find instance whose [runtime_base,+size) contains PC;
  link_pc = link_base + (PC - runtime_base); get_compiled(link_pc); execute with THAT
  instance's relocation base (active_section_base[sec] = runtime_base - link_base), set on
  dispatch (per-context/per-entry) OR a stack-based push/pop override (transitional, fragile
  under cooperative threads).
- single global section_addresses[] is insufficient for >1 live instance of a section.

IMPLEMENTATION ORDER (ChatGPT): 1 historical instance registry; 2 on dispatch-miss resolve
stale target via registry + log link_pc; 3 native lookup runtime-PC -> instance -> link_pc;
4 per-instance reloc base during exec; 5 invalidate only on overlap/free; 6 retest switch-in.

NEXT CONFIRMATION PROBE (cheap, do before the big change): at unhandled_lookup_trampoline,
search a historical instance registry for the stale target (0x8046F950+off) — which gen does
it belong to, does link_pc map to a compiled fn, does current section_addresses point to a
LATER gen? + checksum the old instance bytes at graph-build vs crash. If old bytes INTACT ->
old-instance-dispatch fix confirmed (recoverable). If overwritten/freed -> genuine guest
dangling (allocator/lifetime path, different fix).

## ChatGPT refined verdict #2 (2026-06-22, after frag-INSTANCE registry / +4 finding)
PIVOT: multi-instance address-range dispatch is NO LONGER the first fix. Decisive new facts:
gGeoLayoutCommand = current-frag239(gen38 @0x80553100, size0x32120)END (0x80585220) + 4;
graph pool is allocated immediately AFTER the fragment; the initial segptr fed to
process_geo_layout was ~0x80585220 (pool base / frag end), i.e. ALREADY a graph-pool pointer.
The old frag-base "garbage" targets are downstream artifacts of walking the pool as bytecode.

=> PRIMARY bug: func_80019328's `segptr = temp_v0->unk_08[i]` is ALREADY a built graph-node/
pool pointer when process_geo_layout expects a geo SOURCE pointer. (The destructive
`unk_08[i] = process_geo_layout(unk_08[i])` re-fed.) NOT faithful HW (a geo ptr = frag_end+4
is wrong on HW too); the destructive pattern is safe ONLY if (a) source never processed twice
before reload, OR (b) fragment freshly reloaded before each processing, OR (c) the returned
graph node is the new representation and never re-used as source. The runtime violated one.

TWO CASES (the decisive probe splits them):
- Case A — bad reload/source freshness: the FIRST processing of a fragment generation ALREADY
  has a pool segptr. The "fresh reload" isn't restoring the model data image. Fix = loader/
  canonical-payload path: copy canonical ROM/decompressed payload to dest, relocate from that
  FRESH copy, never reuse the previously-relocated/mutated heap image as source.
- Case B — double-processing: gen starts correct (in-frag segptr), func_80019328 runs a 2nd
  time on the same mutable instance without a fresh reload / without replacing the consumed
  graph. Fix = model/fragment lifetime: don't reprocess a consumed instance, or ensure the
  game-driven reload happens before reprocess.

DEPRIORITIZED: multi-instance dispatch (no resolver makes frag_end+4 valid); main-pool
determinism (would only make garbage less explosive); deep-copy graph (source already stale).

DECISIVE PROBE (next): log func_80019328's segptr (unk_08[i]) at the process_geo_layout call:
frame/thread, frag id+generation, current frag base/end/size, temp_v0, i, unk_08[i] BEFORE
call + classification (inside current frag / inside a historical instance / inside graph pool
/ == frag_end or frag_end+4 / other), first 8-16 bytes at segptr, "has this frag generation
already been processed?", and unk_08[i] AFTER call (returned graph node).
  gen 38 first-seen=YES, before=current-frag, after=graph-node ... then gen 38 again
  first-seen=NO, before=graph-pool -> crash  == Case B (double-processing).
  gen 38 first-seen=YES, before=graph-pool/end+4                  == Case A (bad reload).
ALSO fix the intact-check: snapshot fragment hash + unk_08[] AFTER Memmap_RelocateFragment
(not before), or on the first func(0,0) before any overwrite.

## DECISIVE Case-A determination (2026-06-22, [geoseg] segptr probe)
PSR_GEO_PROBE per-file trace hook on funcs_85.c logged every top-level process_geo_layout
(caller func_80019328 ra~0x8001939C) with segptr (a1) + classification vs gFragments[239].

CRASHING call (only appearance of this frag instance => FIRST processing => CASE A):
  seg=0x8056D8E0 caller=0x8001939C frag239=[0x80553100,0x805667F0) cls=PAST_FRAG_POOL fw=0x0E110000
  - frag239 size = 0x805667F0-0x80553100 = 0x136F0
  - seg offset = 0x8056D8E0-0x80553100 = 0x1A7E0  => 0x70F0 PAST the loaded fragment end.
  - prior occupant of SAME base: frag239=[0x80553100,0x8056D8E0) size=0x1A7E0 (processed fine,
    seg=0x8055FE84 IN_FRAG239). seg(crash) == base + prior_size == prior instance's END.
ALL other 30 [geoseg] entries were IN_FRAG239 (valid). Only the crash one is PAST_FRAG_POOL.

=> ROOT CAUSE (Case A, loader/decompression size): the model fragment (id 239) is loaded /
decompressed to a RUNTIME size (0x136F0) SMALLER than its geo layout references (geo source at
link offset 0x1A7E0). The correctly-relocated geo source pointer (base+0x1A7E0) lands past the
truncated fragment end, in the adjacent graph pool -> process_geo_layout walks pool as bytecode
-> opcode 0x80 -> OOB GeoLayoutJumpTable -> crash. NOT multi-instance dispatch, NOT double-
processing, NOT model lifetime. Intermittent because the short load depends on decompression/
buffer state (the same id loads at many sizes across the run: 0x136F0, 0x1A7E0, 0x24E40, ...).

NEXT: find why frag 239 loads ~0x70F0 short. Path: func_80019204 -> func_800191B0/func_80019170
-> func_80018FF4 (PERS-SZP / PRES-JPEG / raw decompress) -> Memmap_RelocateFragment. Suspect the
recomp's decompress/DMA produces fewer bytes than the link/geo layout, OR sizeInRam vs the geo
data's expected extent mismatch. Confirm by logging loaded size vs expected/decompressed size.

## ChatGPT framework (2026-06-22): Case-A-family confirmed; A-vs-B split + fixes
Both sub-cases produce seg=0x8056D8E0 because runtime_base is identical across the two
same-base instances (0x80553100) and 0x1A7E0 == prior size == the offset. So VALUE alone
can't split them. DECISIVE = compare the CANONICAL (ROM) word for the field, relocated, to
the actual post-reloc value:
  - canonical-relocated == 0x8056D8E0  => CASE A (short-load/bad extent): the model's data
    legitimately references offset 0x1A7E0 but the loaded/registered size is only 0x136F0.
    FIX: loader payload extent — the size used for copy/decompress/relocation/registry/bounds
    must be the FULL resident extent. Invariant: a relocated pointer base+off must lie within
    the registered resident extent (unless an explicit cross-segment ptr). Check do_rom_read/
    decompress length, Memmap_RelocateFragment reloc range, register_runtime_fragment size,
    fragment table size field, rounded/compressed/decompressed-size confusion.
  - canonical-relocated is IN-FRAG but actual == 0x8056D8E0 => CASE B (stale/aliased): the
    load didn't refresh this field from canonical source. FIX: every real fragment load must
    copy from immutable canonical ROM/decompressed bytes into guest RDRAM; never reuse the
    prior runtime heap image or graph-mutated payload as the next load source. Check: is the
    decompressed-fragment cache immutable? was it captured from RDRAM AFTER relocation/mutation?
    does a cache-hit skip the copy but still update gFragments/register?
  - ALSO classify the FIELD ADDRESS (&temp_v0->unk_08[i]): if temp_v0 or field_addr is OUTSIDE
    [current frag), func(0,0) returned a stale/aliased struct pointer and the bad source is
    downstream of that.

DECISIVE PROBE (splits A/B): hook func_80019328 BEFORE process_geo_layout; log frag id/gen,
runtime_base, recorded_size, runtime_end, temp_v0, i, field_addr=&unk_08[i] + class,
unk_08[i] value + class, offset, first16 at unk_08[i], AND canonical-word-relocated vs actual.

## Mid-function aliasing angle (Matthew's intuition) — ChatGPT verdict 2026-06-22
Mid-function/interior-entry dispatch aliasing is a REAL class to audit but SECONDARY here:
the FINAL crash target (0x804EF0E0) was a get_function MISS, not a stale-alias resolve — so a
stale alias is NOT the final dispatch failure (a miss means the entry was unregistered or never
code). BUT it is NOT ruled out UPSTREAM: func_80019328 does `func = arg1; temp_v0 = func(0,0)`
— it calls the model fragment THROUGH A COMPUTED POINTER (get_function(arg1)). If get_function
(arg1) resolves a STALE old-instance entry/alias (wrong relocation/section base), func(0,0)
returns a stale temp_v0 -> stale unk_08[i] -> the downstream pool-walk crash. = the "dangerous
silent case" of incomplete unregister on fragment reload (function starts + interior-entry
aliases + return/resume labels + computed-call aliases for the OLD runtime range must all be
removed when frag 239 moves base).

DECISIVE PROBE (three-way split), log at func_80019328's func(0,0) + process_geo_layout call:
  arg1 (fragment ptr) ; get_function(arg1) result + resolved id/generation + alias kind +
  active section base used ; returned temp_v0 + class ; field_addr=&unk_08[i] + class ;
  unk_08[i] value + class ; canonical-expected word vs actual relocated word ; current frag size.
Outcomes:
  - arg1=current gen, get_function=current entry, temp_v0 in-frag, only unk_08[i] bad,
    expected==actual => LOADER EXTENT / DATA FRESHNESS (Case A/B), aliasing NOT the cause.
  - arg1=current gen, get_function resolves OLD gen alias, temp_v0 outside current frag
    => INTERIOR-ENTRY ALIASING directly implicated (incomplete unregister on reload).
  - arg1 itself is an OLD frag pointer => stale caller MODEL HANDLE, not func_map.
Plus a parallel func_map-events probe for frag239: register/unregister ranges, alias counts,
overlap with prior range, STALE entries remaining after unregister.
FIX per outcome: extent (size for copy/reloc/registry = full resident extent) | fresh-image
copy (canonical ROM/decompressed, never mutated heap) | unregister ALL old-range func_map +
interior-entry aliases on reload (or support multi-instance) | refresh the caller's model handle.

## CONFIRMED (2026-06-22, FUNC_MAP ALIAS SCAN): pervasive stale aliases — Matthew's hypothesis right
At the crash, nearly EVERY superseded fragment instance retains live func_map entries:
  id4 gen1=2433, id0 gen5=250, id239 gen7=58, gen10=322, ALL gen21-39 id239 = 2-4 each, etc.
=> the recomp does NOT fully unregister a fragment instance's func_map entries (function starts
+ interior-entry/computed-call aliases) when the slot reloads/moves. INCOMPLETE UNREGISTER ON
RELOAD is real and widespread across all fragment ids. This is the mid-function/interior-entry
ALIASING class. func_80019328's `temp_v0 = func(0,0)` is a COMPUTED call into the fragment;
a stale alias there can resolve old code -> return stale temp_v0 -> stale unk_08[i] -> the
graph-pool geo source -> crash.

Also: SECTION size vs GAME size mismatch. Registry (section.size) for gen38 @0x80553100 =
0x1A7E0; live gFragments[239].size at the crashing call = 0x136F0 (read by [geoseg]). Disagree
by 0x70F0. Geo source seg=0x8056D8E0 = base + 0x1A7E0 (== section end), past the game fragment
end 0x805667F0. So instance<->section binding/extent is also wrong.

CONVERGENCE: Matthew's aliasing intuition + ChatGPT's earlier multi-instance/address-range
recommendation are the SAME root — the recomp treats fragment id/section as a single LATEST
mapping; it neither fully unregisters superseded instances NOR binds each game instance to the
correct section extent. ROOT CAUSE OF #9 = recomp fragment-INSTANCE lifetime management.

FIX (large, load-bearing, benefits all games): (1) on fragment reload/move, COMPLETELY
unregister the prior instance's func_map entries for its whole runtime range (starts + interior
+ computed-call aliases + return labels); and/or (2) full multi-instance residency + address-
range dispatch + per-instance relocation base (keep each live instance valid until its RDRAM is
freed/overwritten); and (3) bind each game fragment instance to the correct recompiled section
extent (section.size must match the game's loaded fragment extent, no wrong-variant/hash match).
