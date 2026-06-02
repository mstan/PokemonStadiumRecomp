"""Companion to the new rdram_poke debug-server command. Lets us
mutate Vtx coordinates on a live runner to test overlap hypotheses
without rebuilding extras.c + game.toml.

Usage examples:

  # Write 4 bytes at an address
  python tools/_rdram_poke.py 0x8011D460 0102030A

  # Patch the top Y of every quad in a 16-quad column (col-major panel)
  # — increases overlap by +N units on the row seam.
  python tools/_rdram_poke.py --grid 0x8011D460 --quads 16 --col-major-stride 8 --row-overlap +1

The --grid mode reads each quad, computes the per-vertex byte offsets
for V0/V3.y and rewrites them with the requested delta.
"""
import argparse, socket, json, struct, sys


def call(cmd, timeout=5):
    s = socket.create_connection(('127.0.0.1', 4371), timeout=timeout)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    while True:
        try:
            c = s.recv(65536)
        except socket.timeout:
            break
        if not c: break
        buf += c
        if buf.endswith(b'\n'): break
    s.close()
    return json.loads(buf.decode().strip())


def peek(addr, n):
    r = call({'cmd':'rdram_peek', 'addr':addr, 'n':n})
    return bytes.fromhex(r['hex'])


def poke(addr, data):
    r = call({'cmd':'rdram_poke', 'addr':addr, 'hex':data.hex()})
    return r


def grid_patch(base, nquads, rows, cols, row_overlap, col_overlap, col_major):
    """Apply Codex-style overlap to a grid of nquads quads.

    rows*cols must equal nquads. Layout (row-major vs col-major) controls
    how the linear quad index maps to (row, col).
    """
    assert rows * cols == nquads, f'{rows}*{cols} != {nquads}'
    for q in range(nquads):
        if col_major:
            col = q // rows
            row = q % rows
        else:
            row = q // cols
            col = q % cols
        quad_addr = base + q * 0x40
        for v in range(4):
            v_addr = quad_addr + v * 0x10
            raw = peek(v_addr, 16)
            x, y, z, fl, s, t, r, g, b, a = struct.unpack('>hhhhhhBBBB', raw)
            new_x, new_y = x, y
            if row > 0 and v in (0, 3):  # top vertices of the quad
                new_y = y + row_overlap
            if col > 0 and v in (0, 1):  # left vertices of the quad
                new_x = x - col_overlap
            if new_x != x or new_y != y:
                packed = struct.pack('>hhhhhhBBBB',
                                     new_x, new_y, z, fl, s, t, r, g, b, a)
                poke(v_addr, packed)
                print(f'  q{q:2d} v{v}: ({x:5d},{y:5d}) -> ({new_x:5d},{new_y:5d})')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('addr', nargs='?', help='raw poke address (hex)')
    p.add_argument('hex', nargs='?', help='raw hex bytes to write')
    p.add_argument('--grid', help='grid mode: base Vtx address (hex)')
    p.add_argument('--quads', type=int, help='total quads in the grid')
    p.add_argument('--rows', type=int, help='rows in the grid')
    p.add_argument('--cols', type=int, help='cols in the grid')
    p.add_argument('--col-major', action='store_true',
                   help='quad index goes down columns first (default: row-major)')
    p.add_argument('--row-overlap', type=int, default=0,
                   help='additional Y overlap per row seam (+N)')
    p.add_argument('--col-overlap', type=int, default=0,
                   help='additional X overlap per col seam (+N)')
    args = p.parse_args()

    if args.grid:
        base = int(args.grid, 0)
        if not args.rows or not args.cols:
            # auto-detect from quads if only one dim given
            if args.quads:
                if args.col_major:
                    args.cols = args.cols or 1
                    args.rows = args.rows or (args.quads // args.cols)
                else:
                    args.rows = args.rows or 1
                    args.cols = args.cols or (args.quads // args.rows)
            else:
                print('--grid needs --quads + --rows + --cols')
                return 1
        nquads = args.quads or (args.rows * args.cols)
        print(f'Patching grid at 0x{base:08X}: {args.rows}r x {args.cols}c '
              f'({nquads} quads, '
              f'{"col" if args.col_major else "row"}-major), '
              f'row_overlap=+{args.row_overlap}, col_overlap=+{args.col_overlap}')
        grid_patch(base, nquads, args.rows, args.cols,
                   args.row_overlap, args.col_overlap, args.col_major)
        return 0

    if args.addr and args.hex:
        addr = int(args.addr, 0)
        data = bytes.fromhex(args.hex)
        r = poke(addr, data)
        print(r)
        return 0

    p.print_help()
    return 1


if __name__ == '__main__':
    sys.exit(main())
