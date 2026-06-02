"""Targeted check: is fragment4 at 0x80207D20? If yes, the bad pointer
0x80208F20 IS its D_87A01200 — confirming the memory-was-overwritten
hypothesis."""
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


# Direct check: peek 32 bytes at the hypothesized fragment4 base
candidates = [
    0x80207D20,  # base implied by 0x80208F20 - 0x1200
    0x80207000,  # 8KB-aligned floor
    0x80207800,  # midway
    0x80206000,
    0x80205000,
    0x80200000,  # generic landmark
    # If pool-aligned to 0x40 boundary
    0x80207D00,
    0x80207D40,
]

for addr in candidates:
    r = peek(addr, 32)
    if r.get("ok"):
        h = r["hex"]
        words = [h[i:i+8] for i in range(0, len(h), 8)]
        is_frag = ""
        if h[16:32] == "465241474D454E54":
            is_frag = "  <-- FRAGMENT HEADER"
        print(f"0x{addr:08X}: {' '.join(words)}{is_frag}")

# Also check if the bad target itself looks like a fragment header
# (in case fragment4 IS at 0x80208F20 - 0x1200 + some carry)
print()
print("Test: what if HI/LO carry shifted base by 0x10000?")
for addr in (0x80208D20, 0x80218F20, 0x801F8F20):
    r = peek(addr, 32)
    if r.get("ok"):
        h = r["hex"]
        words = [h[i:i+8] for i in range(0, len(h), 8)]
        is_frag = ""
        if h[16:32] == "465241474D454E54":
            is_frag = "  <-- FRAGMENT HEADER"
        print(f"0x{addr:08X}: {' '.join(words)}{is_frag}")
