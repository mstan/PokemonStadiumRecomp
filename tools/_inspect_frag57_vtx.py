"""Dump the Vtx grids that Codex's seam-overlap hook targets, so we
can see which quads cover which UI region. Each grid entry is a 4-vertex
quad (0x40 bytes); each vertex is 16 bytes (xyz / flag / st / rgba).

Usage:  python tools/_inspect_frag57_vtx.py <offset_hex> <count>
"""
import struct, sys

ROM = 'F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64'
FRAG57_ROM_BASE = 0x002C49D0


def dump_grid(off, count):
    with open(ROM, 'rb') as f:
        f.seek(FRAG57_ROM_BASE + off)
        data = f.read(count * 0x40)
    print(f'=== frag57 +0x{off:04X}  ({count} quads, {count*0x40} bytes) ===')
    for q in range(count):
        quad_off = q * 0x40
        print(f'\n  quad #{q:2d} (frag +0x{off + quad_off:04X}):')
        for v in range(4):
            v_off = quad_off + v * 0x10
            x, y, z, flag, s, t, cr, cg, cb, ca = struct.unpack(
                '>hhhhhhBBBB', data[v_off:v_off + 0x10])
            print(f'    V{v}: xyz=({x:5d},{y:5d},{z:5d}) flag={flag:#06x} '
                  f'st=({s/32.0:6.1f},{t/32.0:6.1f}) '
                  f'rgba=({cr:3d},{cg:3d},{cb:3d},{ca:3d})')


if __name__ == '__main__':
    off = int(sys.argv[1], 16) if len(sys.argv) > 1 else 0x7008
    count = int(sys.argv[2]) if len(sys.argv) > 2 else 13
    dump_grid(off, count)
