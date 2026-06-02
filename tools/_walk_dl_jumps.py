"""Walk a captured DL and emit every G_DL / G_BRANCH_Z / G_ENDDL site
and the absolute addresses they target. Helps find sub-DL chains that
weren't followed by the dump_current_dl single-buffer capture.
"""
import struct, sys, collections

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/dl_mainmenu.bin'

with open(PATH, 'rb') as f:
    data = f.read()

ops_seen = collections.Counter()
calls = []
i = 0
while i + 8 <= len(data):
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    ops_seen[op] += 1
    if op == 0xDE:
        # G_DL: w1 = dl addr, w0 bit 16 = branch_z flag
        branch = (w0 >> 16) & 1
        calls.append(('G_DL', i, w1, 'branch' if branch else 'push'))
    elif op == 0xDF:
        calls.append(('G_ENDDL', i, 0, ''))
    elif op == 0xD9:
        calls.append(('G_SETOTHERMODE_H', i, w1, f'h=0x{w1:08x}'))
    elif op == 0xD7:
        calls.append(('G_TEXTURE', i, w1, f'sc={w1:#x}'))
    i += 8

print(f'DL: {PATH}  ({len(data)} bytes)')
print(f'\nOp histogram:')
for op, cnt in sorted(ops_seen.items(), key=lambda kv: -kv[1])[:20]:
    print(f'  0x{op:02X}: {cnt}')

print(f'\nG_DL/G_ENDDL/control ops:')
for kind, off, target, info in calls:
    if kind == 'G_DL':
        seg = (target >> 24) & 0xFF
        print(f'  +{off:#06x}  {kind:10s} -> 0x{target:08X}  seg={seg:#04x}  {info}')
    else:
        print(f'  +{off:#06x}  {kind:10s}  {info}')
