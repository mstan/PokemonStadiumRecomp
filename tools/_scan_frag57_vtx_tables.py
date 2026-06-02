"""Scan fragment 57's data section for likely Vtx tables. Heuristic:
runs of >=12 consecutive Vtx-shaped 16-byte records where xyz is in
[-512, 512], flag is 0, st is in [-1024, 1024]*32, rgba == FFFFFFFF
or some other uniform color.

Goal: enumerate ALL Vtx tables in frag57 so we can cross-reference
against Codex's known-patched list and find the one driving Player
3's residual yellow seams.
"""
import struct, sys

ROM = 'F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64'
FRAG_BASE = 0x002C49D0
FRAG_SIZE = 0x9FF0


def looks_like_vtx(b):
    if len(b) < 16:
        return False
    x, y, z, flag, s, t, r, g, bl, a = struct.unpack('>hhhhhhBBBB', b[:16])
    if flag != 0:
        return False
    if abs(x) > 1000 or abs(y) > 1000 or abs(z) > 1000:
        return False
    # st can be in 5.10 fixed; raw values up to ~32k OK
    if a != 255:
        return False
    return True


def runlen_vtx(data, start, max_n=64):
    n = 0
    while n < max_n and start + (n + 1) * 16 <= len(data):
        if not looks_like_vtx(data[start + n * 16:start + n * 16 + 16]):
            break
        n += 1
    return n


def main():
    with open(ROM, 'rb') as f:
        f.seek(FRAG_BASE)
        data = f.read(FRAG_SIZE)

    runs = []
    i = 0
    while i + 16 <= len(data):
        if looks_like_vtx(data[i:i + 16]):
            n = runlen_vtx(data, i, max_n=256)
            if n >= 12:  # at least 3 quads
                runs.append((i, n))
                i += n * 16
                continue
        i += 4  # finer alignment — Vtx tables can start at +4 within blocks too

    print(f'=== frag57 Vtx-shaped runs (>=12 vertices) ===\n')
    for off, n in runs:
        nquads = n // 4
        # Look at first quad to characterize
        x0, y0, _, _, _, _, _, _, _, _ = struct.unpack('>hhhhhhBBBB', data[off:off + 16])
        last = off + (n - 1) * 16
        xn, yn, _, _, _, _, _, _, _, _ = struct.unpack('>hhhhhhBBBB', data[last:last + 16])
        print(f'  +0x{off:04X}  {n:3d} vtx  ({nquads} quads)  '
              f'first xy=({x0:5d},{y0:5d}) last xy=({xn:5d},{yn:5d})')


if __name__ == '__main__':
    main()
