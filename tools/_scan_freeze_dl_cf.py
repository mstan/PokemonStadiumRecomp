"""Scan the captured freeze DL for control-flow ops likely to hang
RT64::Interpreter::processDisplayLists.

F3DEX2 control-flow opcodes:
  0xDE G_DL          — call sub-DL (cmd[1] = sub-DL pointer; if push=0 -> branch, else call)
  0xDF G_ENDDL       — return from sub-DL
  0xDA G_BRANCHZ     — branch on Z compare
  0xDD G_LOAD_UCODE  — switch ucode (could land in nothing)
  0xE5/E6/E7 G_RDPHALF_2/_CONT/SYNC etc. (data ops, not control flow)

Reports:
  - Total cmds, op histogram
  - All G_DL targets — flag any that point back into the captured DL
    (suggests recursion to a known-good DL, but also catches cycles)
  - G_ENDDL count vs G_DL push=1 count
  - Any G_BRANCH_Z with constant comparand
"""
from __future__ import annotations
import struct
import sys
from collections import Counter

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_freeze_dl.bin"
data = open(PATH, "rb").read()
ncmds = len(data) // 8
print(f"Loaded {len(data)} bytes = {ncmds} cmds from {PATH}")

ops = Counter()
g_dl_targets = []        # (cmd_idx, target_addr, push_flag)
g_branch_targets = []    # (cmd_idx, dl_target, w0)
g_enddl_at = []
last_op = None
last_op_run = 0
runs = []                # (op, count, start_idx) — only runs >= 5

for i in range(ncmds):
    w0, w1 = struct.unpack(">II", data[i*8:i*8+8])
    op = (w0 >> 24) & 0xFF
    ops[op] += 1
    if op == last_op:
        last_op_run += 1
    else:
        if last_op is not None and last_op_run >= 5:
            runs.append((last_op, last_op_run, i - last_op_run))
        last_op = op
        last_op_run = 1
    if op == 0xDE:  # G_DL
        push = (w0 >> 16) & 0xFF  # push=0 -> branch (no return), 1 -> call
        g_dl_targets.append((i, w1, push))
    elif op == 0xDF:  # G_ENDDL
        g_enddl_at.append(i)
    elif op == 0xDA:  # G_BRANCH_Z
        g_branch_targets.append((i, w1, w0))

if last_op is not None and last_op_run >= 5:
    runs.append((last_op, last_op_run, ncmds - last_op_run))

print(f"\n=== op histogram (top 15) ===")
for op, n in ops.most_common(15):
    print(f"  0x{op:02X}  count={n:6d}  ({n*100/ncmds:.1f}%)")

print(f"\n=== runs of identical opcodes (>=5 in a row) ===")
for op, n, start in runs[:40]:
    print(f"  cmds [{start:4d}..{start+n-1:4d}]  op=0x{op:02X}  run={n}")
print(f"  total runs: {len(runs)}")

print(f"\n=== G_DL (0xDE) calls ({len(g_dl_targets)}) — first 20 + last 5 ===")
for idx, tgt, push in g_dl_targets[:20]:
    kind = "CALL (push=1)" if push == 1 else f"BRANCH (push=0x{push:02X})"
    print(f"  cmd[{idx:4d}]  target=0x{tgt:08X}  {kind}")
if len(g_dl_targets) > 25:
    print(f"  ...")
    for idx, tgt, push in g_dl_targets[-5:]:
        kind = "CALL" if push == 1 else f"BRANCH (push=0x{push:02X})"
        print(f"  cmd[{idx:4d}]  target=0x{tgt:08X}  {kind}")

print(f"\n=== G_ENDDL (0xDF) at: {g_enddl_at[:10]}... total={len(g_enddl_at)} ===")
print(f"  G_DL CALLs (push=1): {sum(1 for _,_,p in g_dl_targets if p == 1)}")
print(f"  G_DL BRANCHs (push=0): {sum(1 for _,_,p in g_dl_targets if p == 0)}")
print(f"  expected G_ENDDLs >= G_DL CALLs (each call needs a return)")

print(f"\n=== G_BRANCH_Z ({len(g_branch_targets)}) — first 10 ===")
for idx, tgt, w0 in g_branch_targets[:10]:
    print(f"  cmd[{idx:4d}]  target=0x{tgt:08X}  w0=0x{w0:08X}")

# Look for "outlier" final commands — interpreter hangs typically because
# the LAST processed cmd points back somewhere. Show last 20 cmds.
print(f"\n=== last 20 cmds (where interpreter may be stuck) ===")
for i in range(max(0, ncmds - 20), ncmds):
    w0, w1 = struct.unpack(">II", data[i*8:i*8+8])
    op = (w0 >> 24) & 0xFF
    print(f"  [{i:4d}]  w0=0x{w0:08X} w1=0x{w1:08X}  op=0x{op:02X}")
