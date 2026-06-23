# Issue #16 — gallery photo "enlarge" (C-Down) crash — static analysis

## Symptom
EXCEPTION_ACCESS_VIOLATION, `jalr` to **0x00000000** (null function pointer).
From `_issue_logs/16/last_error.log`:
- `ra = 0x8001BD2C`  → call site is `0x8001BD24`
- at trampoline: `a0=0, a1=0, v0=0, t9=0`
- original `a0 (object)` saved on stack = **0x80200088**, original `a1 = 1`
- user build had CPU trace ring OFF (`trace_recent` empty), so their report has
  no function history. RSP DL interp rings present but irrelevant.

## Mechanism (confirmed from generated C)
`func_8001BD04` (generated funcs_141.c) is the generic "call widget method[10]":
```
lw  v0, 0xC(a0)      ; v0 = widget->0xC   (vtable pointer)
or  a0, zero, zero   ; a0 = 0
or  a1, zero, zero   ; a1 = 0
lw  t9, 0x28(v0)     ; t9 = vtable->0x28  (method index 10)
jalr t9              ; call method(0,0)
```
The recomp translation is FAITHFUL. The bug is in the DATA: `widget->0xC == 0`
(v0=0 at crash) ⇒ `t9 = MEM[0x28] = 0` ⇒ null call. So the widget at 0x80200088
has a **null vtable pointer at +0xC**.

## 0x80200088 is a heap object
Game has a `main_pool` arena allocator (`main_pool_alloc` @ 0x800025C4 etc.).
0x80200088 is above the static program ⇒ a main_pool allocation. So this is a
dynamically-allocated UI widget whose vtable (+0xC) should be set by a constructor.

## Reconstructed guest call stack (from stack windows in the dump)
```
[gallery OVERLAY @ ~0x888xxxxx, ret 0x88803C34]   <- gallery/photo code is a fragment
  -> func_8001ABAC  (0x8001ABAC, resident UI scene/item update loop; frame 0x50, ra@sp+0x24)
     -> func_8001A714 (per-item update; frame 0x40, ra@sp+0x24; a0=M, a1=item)
                       M->0x10 = widget = 0x80200088
        -> func_8001BD04(widget, 1)  -> widget->0xC == 0 -> jalr 0  CRASH
```
Frame math: bd04 sp=0x80081690(-0x18) <- a714 sp=0x800816A8(-0x40) <- abac sp=0x800816E8(-0x50).
Saved ra at abac sp+0x24 = 0x8008170C = **0x88803C34** (overlay).

## Engine context
Build pins N64Recomp **9483814** (has dispatch-entry reject + self-heal interpret
+ interior computed-call dispatch-alias pass). So a plain "undiscovered indirect
entry" would self-heal, NOT crash. ⇒ the null vtable is NOT simply a missed entry.

## Leading hypotheses (need runtime trace to decide)
1. Overlay-relocation: the widget's vtable is an overlay address; constructor's
   store of the vtable resolved/wrote wrong (or to a different addr) leaving +0xC
   at its zero-init value. (Matches prior section_addresses-staleness class.)
2. Use-after-free / stale widget: widget freed (pool zeros +0xC), then re-used by
   the enlarge path which expects it constructed.
3. Construct-order: enlarge updates/draws the widget one frame before its
   constructor sets the vtable.

## Likely shared with #9 ("Choosing Starmie crashes")
Gallery = photographing 3D Pokemon. Choosing a Pokemon (#9) and enlarging the
photo (#16) plausibly share this overlay UI-widget subsystem & null-vtable class.

## Next data (traced build = PSR_TRACE_HOOKS=ON, ring 256K, dump 65536)
- trace_recent at crash: did the widget constructor run this/last frame? which
  overlay fns ran right before func_8001ABAC?
- live rdram_peek 0x80200088 region: widget layout; which fragment is at 0x888xxxxx
  (memmap_ring); what the vtable should be vs is.
