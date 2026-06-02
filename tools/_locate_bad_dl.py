"""Locate the malformed G_DL cmd in the captured RDRAM snapshot.

The bad cmd from interp_cf:
  w0=0xDEFC0E0F  w1=0x5AD12C91   (op=G_DL, push=0xFC=garbage)

Steps:
  1. Find every occurrence of bytes DE FC 0E 0F in last_run_rdram.bin.
  2. For each hit, dump the surrounding 8-byte cmds (16 cmds before / after).
  3. Cross-reference with the 14 G_DL CALL targets in last_run_freeze_dl.bin.
  4. Validate each CALLed sub-DL: walk until G_ENDDL, flag any G_DL whose
     push byte is not 0 or 1.
"""
from __future__ import annotations
import struct
import sys
from pathlib import Path

RDRAM_PATH = Path("build/last_run_rdram.bin")
FREEZE_PATH = Path("build/last_run_freeze_dl.bin")
TARGET = bytes([0xDE, 0xFC, 0x0E, 0x0F])

if not RDRAM_PATH.exists():
    print(f"ERROR: {RDRAM_PATH} not found")
    sys.exit(1)

rdram = RDRAM_PATH.read_bytes()
print(f"Loaded {len(rdram)} bytes RDRAM ({len(rdram)/1024/1024:.2f} MB)")

# 1. Locate every occurrence of the bad cmd's first word.
hits = []
i = 0
while True:
    j = rdram.find(TARGET, i)
    if j < 0:
        break
    # Only count hits at 8-byte alignment (DL cmds are 8-byte aligned).
    if j % 4 == 0:
        # Confirm w1
        w1 = struct.unpack(">I", rdram[j+4:j+8])[0]
        hits.append((j, w1))
    i = j + 1

print(f"\n=== {len(hits)} hits for bytes DE FC 0E 0F (4-byte aligned) ===")
for off, w1 in hits[:20]:
    vaddr = 0x80000000 + off
    print(f"  @ rdram_off=0x{off:08X}  vaddr=0x{vaddr:08X}  w1=0x{w1:08X}")
if len(hits) > 20:
    print(f"  ... ({len(hits) - 20} more)")

# 2. For the most likely hit (w1 matches the interp_cf event = 0x5AD12C91),
#    dump surrounding cmds.
TARGET_W1 = 0x5AD12C91
target_hits = [(off, w1) for off, w1 in hits if w1 == TARGET_W1]
print(f"\n=== {len(target_hits)} hits with EXACT w1=0x{TARGET_W1:08X} ===")
for off, w1 in target_hits:
    vaddr = 0x80000000 + off
    print(f"\n  --- bad cmd @ rdram_off=0x{off:08X}  vaddr=0x{vaddr:08X} ---")
    # Walk 16 cmds before and after.
    start = max(0, off - 16 * 8)
    end = min(len(rdram), off + 17 * 8)
    for k in range(start, end, 8):
        if k + 8 > len(rdram):
            break
        a, b = struct.unpack(">II", rdram[k:k+8])
        op = (a >> 24) & 0xFF
        marker = "  <<<<< BAD" if k == off else ""
        print(f"    @0x{k:08X} (vaddr 0x{0x80000000+k:08X}): "
              f"w0=0x{a:08X} w1=0x{b:08X}  op=0x{op:02X}{marker}")

# 3. Cross-reference with freeze DL CALL targets.
freeze = FREEZE_PATH.read_bytes() if FREEZE_PATH.exists() else b""
ncmds = len(freeze) // 8
g_dl_calls = []
for i in range(ncmds):
    w0, w1 = struct.unpack(">II", freeze[i*8:i*8+8])
    op = (w0 >> 24) & 0xFF
    push = (w0 >> 16) & 0xFF
    if op == 0xDE and push == 1:
        g_dl_calls.append((i, w1))

print(f"\n=== Cross-reference with freeze DL CALLs (push=1) ===")
print(f"  Total CALLs: {len(g_dl_calls)}")
for cmd_idx, target_vaddr in g_dl_calls:
    target_off = target_vaddr - 0x80000000
    if target_off < 0 or target_off >= len(rdram):
        print(f"  cmd[{cmd_idx:4d}] target 0x{target_vaddr:08X} OUT OF RDRAM RANGE")
        continue
    # Check whether any of the bad-cmd hits fall within ~64 KB downstream
    # of this CALL target (i.e., is the bad cmd inside this sub-DL?).
    nearby = [off for off, _ in target_hits
              if 0 <= (off - target_off) <= 0x10000]
    print(f"  cmd[{cmd_idx:4d}] CALL -> vaddr=0x{target_vaddr:08X} "
          f"rdram_off=0x{target_off:08X}  nearby_bad_hits={nearby!r}")

# 4. Walk each CALLed sub-DL until G_ENDDL or 1024 cmds, flag any G_DL
#    whose push isn't 0 or 1.
print(f"\n=== Validate each CALLed sub-DL (walk for G_ENDDL or weird push) ===")
for cmd_idx, target_vaddr in g_dl_calls:
    target_off = target_vaddr - 0x80000000
    if target_off < 0 or target_off >= len(rdram):
        continue
    print(f"\n  --- sub-DL from CALL cmd[{cmd_idx}] @ vaddr=0x{target_vaddr:08X} ---")
    found_enddl = False
    weird_push_count = 0
    for k in range(target_off, min(target_off + 1024 * 8, len(rdram)), 8):
        if k + 8 > len(rdram):
            break
        a, b = struct.unpack(">II", rdram[k:k+8])
        op = (a >> 24) & 0xFF
        if op == 0xDF:
            found_enddl = True
            ncmds_walked = (k - target_off) // 8
            print(f"    G_ENDDL found at offset +{ncmds_walked} ({ncmds_walked*8} bytes)")
            break
        if op == 0xDE:
            push = (a >> 16) & 0xFF
            if push not in (0, 1):
                weird_push_count += 1
                if weird_push_count <= 3:
                    print(f"    !! BAD push @ +{(k-target_off)//8}: "
                          f"w0=0x{a:08X} (push=0x{push:02X}) w1=0x{b:08X}")
    if not found_enddl:
        print(f"    NO G_ENDDL within 1024 cmds (likely walks into garbage)")
    if weird_push_count:
        print(f"    {weird_push_count} G_DL cmds with weird push values")
