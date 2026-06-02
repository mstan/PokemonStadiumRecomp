"""Compare what RT64 saw at the bad G_DL vs what's in RDRAM NOW (at dump
time). Both reads are from after the freeze started, so this answers:
  - Is the parent DL itself stable post-freeze?
  - Does the bad target buffer match what walk saw?

Both should agree (both are post-freeze) UNLESS Stadium's game thread
writes past the freeze.
"""
import json, struct
from pathlib import Path

d = json.load(open("build/last_run_report.json"))
rdram = Path("build/last_run_rdram.bin").read_bytes()

# The bad walk event we already know about:
PARENT_VADDR = 0x801A0390
TARGET_VADDR = 0x80208F20
WALK_HEAD_HEX = "D19BD19BD193D19BD193D193D19BD193"   # from prior dump

parent_off = PARENT_VADDR - 0x80000000
target_off = TARGET_VADDR - 0x80000000

# Parent DL cmd at 0x801A0390 — what does it look like NOW?
print(f"=== Parent DL cmd @ vaddr 0x{PARENT_VADDR:08X} (RDRAM offset 0x{parent_off:08X}) ===")
cmd = rdram[parent_off:parent_off + 8]
w0, w1 = struct.unpack(">II", cmd)
op = (w0 >> 24) & 0xFF
push = (w0 >> 16) & 0xFF
print(f"  bytes: {cmd.hex().upper()}")
print(f"  decoded: w0=0x{w0:08X} w1=0x{w1:08X}  op=0x{op:02X} push={push}")
expected_op = (0xDE)
expected_push = 1
expected_target = 0x80208F20
match = (op == expected_op and push == expected_push and w1 == expected_target)
print(f"  matches expected G_DL push=1 -> 0x80208F20? {match}")

# Target buffer at 0x80208F20 — what does walk side say vs what's NOW?
print(f"\n=== Target buffer @ vaddr 0x{TARGET_VADDR:08X} (RDRAM offset 0x{target_off:08X}) ===")
now_head = rdram[target_off:target_off + 16]
print(f"  walk_head (RT64 saw this at walk time):  {WALK_HEAD_HEX}")
print(f"  now_head  (RDRAM at dump time):          {now_head.hex().upper()}")
match = now_head.hex().upper() == WALK_HEAD_HEX
print(f"  identical? {match}")

# Look at the broader region — a typical sub-DL would have aligned
# G_RDPPIPESYNC / G_SETOTHERMODE etc. Pixel data has small repeating
# 16-bit patterns.
print(f"\n=== Broader target region (32 cmds = 256 bytes from 0x{TARGET_VADDR:08X}) ===")
for i in range(32):
    k = target_off + i * 8
    a, b = struct.unpack(">II", rdram[k:k+8])
    op = (a >> 24) & 0xFF
    print(f"  +0x{i*8:03X}: w0=0x{a:08X} w1=0x{b:08X}  op=0x{op:02X}")
