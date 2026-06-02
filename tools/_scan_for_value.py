"""Scan all 8MB of rdram for occurrences of a specific 32-bit value
stored at any 4-byte-aligned address. Uses existing rdram_peek 256-byte
chunks, so scans the whole region in ~3 minutes."""
import json, socket, sys, struct, time

ADDR = ("127.0.0.1", 4371)


def peek(vaddr, n):
    s = socket.create_connection(ADDR, timeout=10)
    s.sendall((json.dumps({"cmd": "rdram_peek", "addr": vaddr, "n": n}) + "\n").encode())
    buf = b""
    while True:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode())


WANT = 0x80208F20 if len(sys.argv) < 2 else int(sys.argv[1], 0)
print(f"Scanning rdram for stored u32 value 0x{WANT:08X}")

want_bytes = struct.pack(">I", WANT)
hits = []
START = 0x80000000
END = 0x80800000
CHUNK = 256
total = END - START
t0 = time.time()
last_log = t0
for off in range(0, total, CHUNK):
    if time.time() - last_log > 5:
        elapsed = time.time() - t0
        progress = off / total
        eta = elapsed / max(progress, 1e-6) - elapsed
        print(f"  0x{START + off:08X}  {progress*100:.1f}%  hits={len(hits)}  eta={eta:.0f}s", flush=True)
        last_log = time.time()
    r = peek(START + off, CHUNK)
    if not r.get("ok"):
        continue
    h = r["hex"]
    raw = bytes.fromhex(h)
    pos = 0
    while True:
        idx = raw.find(want_bytes, pos)
        if idx < 0: break
        if idx % 4 == 0:  # word-aligned
            hits.append(START + off + idx)
        pos = idx + 1

elapsed = time.time() - t0
print(f"\nScan complete in {elapsed:.1f}s. {len(hits)} hits.")
print()
for h in hits[:50]:
    print(f"  0x{h:08X}")
