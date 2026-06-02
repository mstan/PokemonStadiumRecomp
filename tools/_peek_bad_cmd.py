"""Read bytes around the bad G_DL command location from live rdram."""
import json, socket, sys

ADDR = ("127.0.0.1", 4371)


def peek(vaddr, n):
    s = socket.create_connection(ADDR, timeout=5)
    s.sendall((json.dumps({"cmd": "rdram_peek", "addr": vaddr, "n": n}) + "\n").encode())
    buf = b""
    while True:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode())


# Bad cmd was at step 4364, host pc=3035546227064, rdram_host_base=3035544092672
# rdram offset = 0x208E78 -> N64 vaddr 0x80208E78
target = 0x80208E78
print(f"--- Probing 0x{target:08X} +/- window ---")
for off in (-0x40, -0x20, 0, 0x20, 0x40, 0x80):
    addr = target + off
    r = peek(addr, 32)
    if not r.get("ok"):
        print(f"  0x{addr:08X}: {r}")
        continue
    h = r["hex"]
    # split into 4-byte words
    words = [h[i:i+8] for i in range(0, len(h), 8)]
    print(f"  0x{addr:08X}: {' '.join(words)}")

# Also peek dl_start
print(f"\n--- DL start 0x80191FC0 (first 64 bytes) ---")
r = peek(0x80191FC0, 64)
if r.get("ok"):
    h = r["hex"]
    words = [h[i:i+8] for i in range(0, len(h), 8)]
    print(f"  0x80191FC0: {' '.join(words)}")

# Where the interp is currently walking (recent_pcs centered around offset 0x600..0x2670)
print(f"\n--- Walk region 0x80000600 (first 64 bytes) ---")
r = peek(0x80000600, 64)
if r.get("ok"):
    h = r["hex"]
    words = [h[i:i+8] for i in range(0, len(h), 8)]
    print(f"  0x80000600: {' '.join(words)}")
