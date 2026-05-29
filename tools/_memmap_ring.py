#!/usr/bin/env python3
# Reader for the Memmap segment/fragment binding ring (debug-server cmd
# `memmap_ring`). Each entry is one mutation of gSegments[]/gFragments[]
# in memmap.c, captured via game.toml entry hooks on the four mutators.
#
# Built to diagnose the menu cursor/icon corruption: RSP segment 1
# resolving icon-texture loads into a stale/wrong fragment's code. The
# timeline shows which code bound segment N to what across a screen
# transition, and whether a menu re-bound it or inherited a stale value.
#
# Usage:
#   python tools/_memmap_ring.py [count]            # full timeline
#   python tools/_memmap_ring.py [count] --seg N    # only segment id N events
#   python tools/_memmap_ring.py [count] --segs     # only segment (not fragment) events
import socket, json, sys

HOST = ('127.0.0.1', 4371)
KIND = {0: 'SET_SEG ', 1: 'CLR_SEG ', 2: 'SET_FRAG', 3: 'CLR_FRAG'}


def fetch(count):
    s = socket.create_connection(HOST, timeout=8)
    s.sendall((json.dumps({'cmd': 'memmap_ring', 'count': count}) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())


def main():
    args = sys.argv[1:]
    seg_only = '--segs' in args
    seg_filter = None
    if '--seg' in args:
        seg_filter = int(args[args.index('--seg') + 1])
    pos = [a for a in args if not a.startswith('--')
           and (seg_filter is None or a != str(seg_filter))]
    count = int(pos[0]) if pos else 512

    r = fetch(count)
    if not r.get('ok'):
        print('ERR', r)
        return
    entries = r.get('entries', [])
    print(f"write_idx={r.get('write_idx')} capacity={r.get('capacity')} "
          f"got={len(entries)}")
    for e in entries:
        kind = e['kind']
        if seg_only and kind > 1:
            continue
        if seg_filter is not None and not (kind <= 1 and e['id'] == seg_filter):
            continue
        print(f"  seq={e['seq']:6d} {KIND.get(kind, '?'):>8s} "
              f"id={e['id']:<3d} vaddr={e['vaddr']:#010x} "
              f"size={e['size']:#08x} caller={e['caller']:#010x} "
              f"gs={e['gs']:#06x}")


if __name__ == '__main__':
    main()
