"""Read gSegments[16] from live rdram (0x800A5870, 16x8 bytes).
Each entry is {uintptr_t vaddr, size_t size}.
"""
import json, socket

def peek(vaddr, n):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=5)
    s.sendall((json.dumps({"cmd": "rdram_peek", "addr": vaddr, "n": n}) + "\n").encode())
    buf = b""
    while True:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode())


GSEGMENTS = 0x800A5870
ENTRY_SIZE = 8  # vaddr (4) + size (4)
N = 16

# Read all 128 bytes
r = peek(GSEGMENTS, ENTRY_SIZE * N)
if not r.get("ok"):
    raise SystemExit(f"peek failed: {r}")

h = r["hex"]
print(f"=== gSegments[16] @ 0x{GSEGMENTS:08X} ===")
for i in range(N):
    off = i * ENTRY_SIZE * 2
    vaddr = int(h[off:off+8], 16)
    size = int(h[off+8:off+16], 16)
    note = ""
    if vaddr == 0:
        note = "  (unset)"
    elif 0x80000000 <= vaddr < 0x80800000:
        # Compute the result for our specific bad lookup
        if i == 1:
            target = vaddr + 0x002480
            note = f"  -> seg+0x2480 = 0x{target:08X}"
            if target == 0x80208F20:
                note += "  <<< MATCHES bad target! >>>"
    print(f"  seg[{i:2d}]: vaddr=0x{vaddr:08X}  size=0x{size:08X}{note}")
