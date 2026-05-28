#!/usr/bin/env python3
# Reader for the RT64 sprite-draw ring (debug-server cmd `sprite_ring`).
# Each entry is one drawTexRect: on-screen rect + tile + texture source
# RDRAM address. Built to pinpoint the menu cursor/icon sprite: move the
# menu selection and the cursor's rect moves while everything else stays.
#
#   python tools/_sprite_ring.py [count]                  # dump
#   python tools/_sprite_ring.py [count] --snapshot FILE  # save src->rect
#   python tools/_sprite_ring.py [count] --diff FILE      # find moved sprites
#
# Protocol to find the cursor:
#   1. on the menu (selection row A): --snapshot build/_spr.json
#   2. move selection to row B
#   3. --diff build/_spr.json  -> the src_addr whose rect MOVED is the cursor;
#      that src_addr is its texture source, to look up in the tmem ring.

import socket, json, sys, os
from collections import defaultdict

HOST, PORT = "127.0.0.1", 4371


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "sprite_ring", "count": count}) + "\n").encode())
    buf = b""
    while b"\n" not in buf and b"]}" not in buf:
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())


def src_rect_map(entries):
    # Most recent rect per src_addr in the window.
    m = {}
    for e in entries:
        m[e["src_addr"]] = (e["ulx"], e["uly"], e["lrx"], e["lry"], e["tile"])
    return m


def main():
    args = sys.argv[1:]
    snap = args[args.index("--snapshot") + 1] if "--snapshot" in args else None
    diff = args[args.index("--diff") + 1] if "--diff" in args else None
    pos = [a for a in args if not a.startswith("--") and a not in (snap, diff)]
    count = int(pos[0]) if pos else 512

    data = fetch(count)
    if not data.get("ok", False):
        print("ERROR:", data)
        return
    entries = data.get("entries", [])
    print(f"write_idx={data.get('write_idx')} got={len(entries)} texrects")

    if snap:
        m = {hex(k): v for k, v in src_rect_map(entries).items()}
        with open(snap, "w") as f:
            json.dump(m, f)
        print(f"snapshot: {len(m)} src->rect -> {snap}")
        return
    if diff:
        if not os.path.exists(diff):
            print(f"ERROR: {diff} not found")
            return
        prev = json.load(open(diff))
        cur = {hex(k): v for k, v in src_rect_map(entries).items()}
        moved = [k for k in (set(prev) & set(cur))
                 if list(prev[k][:4]) != list(cur[k][:4])]
        print(f"=== moved sprites (rect changed between captures) ===")
        print(f"common src={len(set(prev) & set(cur))} moved={len(moved)}")
        for k in moved:
            print(f"  ** src={k} tile={cur[k][4]} "
                  f"rect {tuple(prev[k][:4])} -> {tuple(cur[k][:4])}  <-- cursor candidate")
        if not moved:
            print("  (nothing moved — selection may not have changed, or the "
                  "cursor is drawn via textured tris, not texrect)")
        return

    for e in entries:
        print(f"seq={e['seq']:6d} rect=({e['ulx']:4d},{e['uly']:4d})-"
              f"({e['lrx']:4d},{e['lry']:4d}) tile={e['tile']} "
              f"src={e['src_addr']:#09x}")


if __name__ == "__main__":
    main()
