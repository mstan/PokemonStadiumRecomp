"""Walk a captured DL bin and list every G_VTX (op 0x01) call with its
source segment-relative address, count, and vertex-buffer base. Goal: find
ALL Vtx tables that frag57 streams during the Game Pak Check screen so we
can compare against Codex's known-patched list.
"""
import struct, sys, collections

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/current_gamepak_dl.bin'

with open(PATH, 'rb') as f:
    data = f.read()

print(f'DL: {PATH}  ({len(data)} bytes)')

vtx_addrs = collections.Counter()
ops = collections.Counter()

i = 0
while i + 8 <= len(data):
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    ops[op] += 1
    if op == 0x01:
        # G_VTX (F3DEX2): w1 = src vaddr, w0 = (n<<12) | (v0_idx*2)
        n = (w0 >> 12) & 0xFF
        v0 = ((w0 >> 1) & 0x7F)
        vtx_addrs[(w1, n, v0)] += 1
    i += 8

print(f'\nOpcodes used:')
for op, cnt in sorted(ops.items(), key=lambda kv: -kv[1])[:10]:
    print(f'  0x{op:02X}: {cnt}')

print(f'\nUnique G_VTX calls ({len(vtx_addrs)}):')
for (addr, n, v0), cnt in sorted(vtx_addrs.items()):
    in_frag57 = (addr & 0xFF000000) == 0x82000000 or (addr >> 24) == 0x82
    marker = '  [frag57]' if (addr & 0xFFFF0000) == 0x82D00000 else ''
    print(f'  addr=0x{addr:08X}  n={n:2d}  v0={v0:2d}  count={cnt}{marker}')
