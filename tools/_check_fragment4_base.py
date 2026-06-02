"""Check whether fragment4 is currently loaded at 0x80207D20
(which would make D_87A01200 = 0x80208F20).

Also scan a wider region for FRAGMENT magic to find ALL loaded
fragments in the 0x80100000-0x80600000 range.
"""
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


def hex_words(h):
    return [h[i:i+8] for i in range(0, len(h), 8)]


# Check fragment4 hypothesis: if base = 0x80207D20, header is +0x00..0x10
print("=== Hypothesized fragment4 base = 0x80207D20 ===")
r = peek(0x80207D20, 16)
if r.get("ok"):
    print(f"  bytes: {' '.join(hex_words(r['hex']))}")
    if r['hex'][16:32] == "465241474D454E54":
        print("  MAGIC MATCH! fragment4 IS at 0x80207D20")
    else:
        print(f"  no magic, value at +0x08: {r['hex'][16:32]}")

# Try ALSO 0x80208F20 - 0x1200 - some_other_offset
# Maybe HI/LO carry: 0x80208F20 - 0x1200 = 0x80207D20  — that's what we tried
# Try 0x80208F20 - 0x9220 = 0x801FFD00
# Try 0x80208F20 - some other offset

# Search for FRAGMENT magic at every 0x10 offset between 0x80100000 and 0x80600000
# Use 0x100 step for speed
print("\n=== Scan for FRAGMENT magic in 0x80200000-0x80300000 (16-byte step) ===")
hits = []
for addr in range(0x80200000, 0x80300000, 0x10):
    r = peek(addr + 0x08, 8)
    if r.get("ok") and r['hex'] == "465241474D454E54":
        hits.append(addr)
        print(f"  HIT 0x{addr:08X}")

print(f"  total in 0x80200000-0x80300000: {len(hits)}")

# Also scan 0x80300000-0x80600000
print("\n=== Scan 0x80300000-0x80600000 ===")
for addr in range(0x80300000, 0x80600000, 0x10):
    r = peek(addr + 0x08, 8)
    if r.get("ok") and r['hex'] == "465241474D454E54":
        hits.append(addr)
        print(f"  HIT 0x{addr:08X}")

print(f"  TOTAL hits: {len(hits)}")
print()

# For each hit, read header info: J-trampoline target tells us link vram, then which fragment
print("=== Decode each hit to identify fragment ===")
for addr in hits:
    # +0x00 = J target | +0x10 = entry_off | +0x14..+0x17 = link_vram derivation
    r = peek(addr, 32)
    if r.get("ok"):
        h = r['hex']
        j_word = int(h[0:8], 16)
        # MIPS J: top 6 bits = 0x02 (010000), low 26 bits = (target >> 2) & 0x3FFFFFF
        j_target = ((j_word & 0x3FFFFFF) << 2)  # high 4 bits assumed match PC region
        magic_a = h[16:24]
        magic_b = h[24:32]
        print(f"  0x{addr:08X}: J target raw=0x{j_target:08X} magic_a={magic_a} magic_b={magic_b}")
        # Read +0x10..+0x18 for entry_off
        r2 = peek(addr + 0x10, 16)
        if r2.get("ok"):
            print(f"    +0x10..0x1F: {' '.join(hex_words(r2['hex']))}")
