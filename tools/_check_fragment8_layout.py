"""Find fragment8 by checking known anchor points in rdram."""
import json, socket

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


# Show rdram around 0x80200000, 0x801A0000, 0x80100000 — coarse landmarks
# Fragment header layout: +0x00 J, +0x04 nop, +0x08 'FRAG', +0x0C 'MENT'
print("--- 16-byte windows at potential fragment bases ---")
for base in (0x80200000, 0x801F0000, 0x80100000, 0x80190000, 0x801A0000,
             0x80180000, 0x80170000, 0x80160000, 0x80150000, 0x80140000,
             0x80130000, 0x80120000, 0x801D0000, 0x801E0000):
    r = peek(base, 16)
    if r.get("ok"):
        h = r["hex"]
        words = [h[i:i+8] for i in range(0, len(h), 8)]
        annotated = ""
        if h[16:24] == "46524147" and h[24:32] == "4D454E54":
            annotated = "  <-- FRAGMENT HEADER"
        print(f"  0x{base:08X}: {' '.join(words)}{annotated}")

# Also check the bad pointer destination 0x80208F20 +/- search for FRAG within 0x80100000..0x80300000
print()
print("--- step=0x100 search for FRAG in 0x80100000..0x80300000 ---")
hits = []
for addr in range(0x80100000, 0x80300000, 0x100):
    r = peek(addr + 0x08, 4)
    if r.get("ok") and r["hex"] == "46524147":
        hits.append(addr)
        print(f"  HIT 0x{addr:08X}")
print(f"\n{len(hits)} hits")
