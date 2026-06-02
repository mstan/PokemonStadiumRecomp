"""Scan the captured hung DL for every G_DL target. Looking for any
target near the runaway vaddr (recent_pcs were at 0x80002638..0x80002770)."""
import struct
import collections

DL_START_VADDR = 0x80191FC0
with open('F:/Projects/n64recomp/PokemonStadiumRecomp/build/softlock_hung_dl.bin', 'rb') as f:
    data = f.read()

print(f'scanning {len(data)} bytes')
gdl_targets = []  # (offset, w0, w1, push)
endddl_offsets = []
for i in range(0, len(data) - 8, 8):
    w0, w1 = struct.unpack('>II', data[i:i+8])
    op = (w0 >> 24) & 0xFF
    if op == 0xDE:
        push = (w0 >> 16) & 0xFF
        gdl_targets.append((i, w0, w1, push))
    elif op == 0xDF:
        endddl_offsets.append(i)

print(f'G_DL count: {len(gdl_targets)}')
print(f'G_ENDDL count: {len(endddl_offsets)}')

# Bucket targets by high 12 bits to see distribution
buckets = collections.Counter()
for off, w0, w1, push in gdl_targets:
    high = (w1 >> 20) & 0xFFF
    buckets[high] += 1

print('\nG_DL target buckets (high 12 bits):')
for high, cnt in sorted(buckets.items(), key=lambda kv: -kv[1])[:20]:
    print(f'  0x{high:03X}xxxxx: {cnt}')

# Show targets that fall outside the captured DL and aren't in typical fragment ranges
print('\nG_DL targets in LOW RDRAM (< 0x80100000) — RT64 is walking 0x80002xxx now:')
for off, w0, w1, push in gdl_targets:
    if w1 < 0x80100000 and w1 >= 0x80000000:
        print(f'  +{off:#06x}  w0=0x{w0:08X} w1=0x{w1:08X} push={push}  target=0x{w1:08X}')

# Also show segmented (high bits = 0x000 or low):
print('\nG_DL targets with high byte = 0x00 (segmented?):')
for off, w0, w1, push in gdl_targets:
    if w1 < 0x01000000:
        print(f'  +{off:#06x}  w0=0x{w0:08X} w1=0x{w1:08X} push={push}  target=0x{w1:08X}')
