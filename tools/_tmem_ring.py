#!/usr/bin/env python3
# Reader for the RT64 TMEM-load ring (debug-server cmd `tmem_ring`).
# Each entry is one RDP texture-load (loadTile/loadBlock/loadTLUT) at the
# moment RT64 fills TMEM from RDRAM, with the source address + a content
# hash of the loaded bytes.
#
# Built to diagnose the menu cursor/icon sprite corruption (different
# sprites garble on different visits). Key views:
#   - per-TMEM-region: which source addresses / content hashes landed in
#     each TMEM destination. A region that receives DIFFERENT content
#     hashes from the SAME logical sprite across visits = stale/UAF.
#   - source-address spread: a sprite whose load src_addr jumps around or
#     points outside a plausible texture pool = freed-source UAF.
#
# Usage:
#   python tools/_tmem_ring.py [count]            # dump recent loads
#   python tools/_tmem_ring.py [count] --by-tmem  # group by TMEM region
#
# Ring is ALWAYS-ON; query backward over the window of interest. Disable
# the writer with RT64_TMEM_RING_DISABLE=1 in the runner env.

import socket, json, sys, os
from collections import defaultdict, Counter

HOST, PORT = "127.0.0.1", 4371


def src_hash_map(entries):
    # Within one visit each source reloads identical bytes, so collapse to
    # one hash per src_addr (the most common, in case of a rare tear).
    by = defaultdict(Counter)
    meta = {}
    for e in entries:
        by[e["src_addr"]][e["hash"]] += 1
        meta[e["src_addr"]] = {"fmt": e["fmt"], "bytes": e["src_bytes"]}
    return {hex(s): {"hash": c.most_common(1)[0][0], **meta[s]} for s, c in by.items()}


def do_snapshot(entries, path):
    m = src_hash_map(entries)
    with open(path, "w") as f:
        json.dump(m, f, indent=0)
    print(f"snapshot: {len(m)} distinct sources -> {path}")


def do_diff(entries, path):
    if not os.path.exists(path):
        print(f"ERROR: snapshot {path} not found (run --snapshot first)")
        return
    with open(path) as f:
        prev = json.load(f)
    cur = src_hash_map(entries)
    prev_k, cur_k = set(prev), set(cur)
    changed = sorted(k for k in (prev_k & cur_k) if prev[k]["hash"] != cur[k]["hash"])
    print(f"=== cross-visit diff vs {path} ===")
    print(f"prev sources={len(prev)} cur sources={len(cur)} "
          f"common={len(prev_k & cur_k)} new={len(cur_k - prev_k)} "
          f"gone={len(prev_k - cur_k)}")
    print(f"** {len(changed)} sources CHANGED CONTENT at the same address "
          f"(backing memory scribbled between visits = UAF):")
    for k in changed:
        print(f"     src={k} fmt={cur[k]['fmt']} bytes={cur[k]['bytes']}  "
              f"{prev[k]['hash']} -> {cur[k]['hash']}")
    if not changed:
        print("  (no same-address content change — pool stable across these "
              "two captures; corruption may need more re-entries to accumulate)")


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "tmem_ring", "count": count}) + "\n").encode())
    buf = b""
    while b"\n" not in buf and b"]}" not in buf:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf.decode().strip())


def main():
    args = sys.argv[1:]
    by_tmem = "--by-tmem" in args
    by_src = "--by-src" in args
    snap_path = None
    diff_path = None
    if "--snapshot" in args:
        snap_path = args[args.index("--snapshot") + 1]
    if "--diff" in args:
        diff_path = args[args.index("--diff") + 1]
    pos = [a for a in args if not a.startswith("--")
           and a not in (snap_path, diff_path)]
    count = int(pos[0]) if pos else 2048

    data = fetch(count)
    if not data.get("ok", False):
        print("ERROR:", data)
        return
    entries = data.get("entries", [])
    print(f"write_idx={data.get('write_idx')} capacity={data.get('capacity')} "
          f"got={len(entries)} loads")

    if snap_path:
        do_snapshot(entries, snap_path)
        return
    if diff_path:
        do_diff(entries, diff_path)
        return

    if not by_src:
        for e in entries:
            print(f"seq={e['seq']:6d} {e['op']:>5s} fmt={e['fmt']} siz={e['siz']} "
                  f"tmem={e['tmem_addr']:#06x}+{e['tmem_bytes']:<4d} "
                  f"src={e['src_addr']:#09x}+{e['src_bytes']:<5d} "
                  f"img={e['img_addr']:#09x} {e['width']}x{e['rows']} "
                  f"hash={e['hash']}")

    if by_src:
        # The static menu reloads every sprite each frame from a fixed
        # source. A source whose CONTENT HASH varies across the window is
        # having its backing memory mutated underneath it — the UAF/race
        # signature for the corrupt sprite. Group by src_addr; flag any
        # with >1 distinct hash.
        bysrc = defaultdict(list)
        for e in entries:
            bysrc[e["src_addr"]].append(e)
        unstable = []
        for src, evs in bysrc.items():
            hashes = {ev["hash"] for ev in evs}
            if len(hashes) > 1:
                unstable.append((src, evs, hashes))
        print(f"\n=== by source: {len(bysrc)} distinct sources, "
              f"{len(unstable)} with UNSTABLE content (mutating backing memory) ===")
        # Sort unstable by appearance count (most-reloaded first).
        for src, evs, hashes in sorted(unstable, key=lambda t: -len(t[1])):
            e0 = evs[0]
            print(f"  ** src={src:#09x} fmt={e0['fmt']} bytes={e0['src_bytes']} "
                  f"appears {len(evs)}x with {len(hashes)} distinct hashes:")
            seen = []
            for ev in evs:
                if ev["hash"] not in [h for _, h in seen]:
                    seen.append((ev["seq"], ev["hash"]))
            for seq, h in seen[:8]:
                print(f"        seq={seq:7d} hash={h}")
        if not unstable:
            print("  (no source had varying content this window — backing "
                  "memory is stable; corruption is not an in-window source mutation)")

    if by_tmem:
        # Group by TMEM destination; flag regions that received >1 distinct
        # content hash (a sprite slot whose contents changed = prime
        # stale-TMEM / UAF suspect).
        regions = defaultdict(list)
        for e in entries:
            regions[e["tmem_addr"]].append(e)
        print("\n=== by TMEM region (** = multiple distinct content hashes) ===")
        for tmem in sorted(regions):
            evs = regions[tmem]
            hashes = {ev["hash"] for ev in evs}
            srcs = {ev["src_addr"] for ev in evs}
            flag = " ** MULTI-HASH" if len(hashes) > 1 else ""
            print(f"  tmem={tmem:#06x}: {len(evs)} loads, "
                  f"{len(hashes)} distinct hash, {len(srcs)} distinct src{flag}")
            if len(hashes) > 1:
                for ev in evs:
                    print(f"      seq={ev['seq']:6d} src={ev['src_addr']:#09x} "
                          f"hash={ev['hash']} {ev['op']}")


if __name__ == "__main__":
    main()
