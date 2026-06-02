"""Count and locate every G_DL command in the failing DL whose target
falls into audio memory range (0x80200000-0x80300000)."""
import struct
from collections import Counter

data = open("build/failing_dl.bin", "rb").read()
DL_START = 0x80191FC0

print(f"DL size: {len(data)} bytes")
print()
print("=== ALL G_DL targets in 0x80200000-0x80300000 (audio range) ===")
audio_count = 0
audio_targets = []
for off in range(0, len(data) - 8, 8):
    w0, w1 = struct.unpack(">II", data[off:off+8])
    if (w0 >> 24) == 0xDE and 0x80200000 <= w1 < 0x80300000:
        audio_count += 1
        audio_targets.append((off, w0, w1))
        if audio_count <= 30:
            print(f"  +0x{off:05X} (vaddr 0x{DL_START + off:08X}): w0=0x{w0:08X} w1=0x{w1:08X}")
print(f"total bad: {audio_count}")
print()

print("=== count of unique G_DL targets (top 20) ===")
targets = Counter()
for off in range(0, len(data) - 8, 8):
    w0, w1 = struct.unpack(">II", data[off:off+8])
    if (w0 >> 24) == 0xDE:
        targets[w1] += 1
print(f"unique targets: {len(targets)}")
for t, c in targets.most_common(20):
    marker = " <-- AUDIO" if 0x80200000 <= t < 0x80300000 else ""
    print(f"  0x{t:08X}: {c} times{marker}")

# Also search for the SETENVCOLOR + G_DL pattern WITH audio target
print()
print("=== G_SETENVCOLOR(94) immediately followed by G_DL audio target ===")
for off in range(0, len(data) - 16, 8):
    w0, w1 = struct.unpack(">II", data[off:off+8])
    w2, w3 = struct.unpack(">II", data[off+8:off+16])
    if (w0 == 0xFB000000 and w1 == 0x00000094 and
        (w2 >> 24) == 0xDE and 0x80200000 <= w3 < 0x80300000):
        print(f"  +0x{off:05X} pair: SETENVCOLOR(0x94) at vaddr 0x{DL_START + off:08X}, "
              f"G_DL target 0x{w3:08X}")
