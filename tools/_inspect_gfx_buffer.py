"""Inspect the gfx workspace around 0x801A0390 to understand the
DL builder layout. We need to find the higher-level G_DL call that
wrote 0x80208F20 as a target.
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


def dump_window(label, base, length=256):
    print(f"\n=== {label}  base=0x{base:08X}  len={length} ===")
    for off in range(0, length, 16):
        r = peek(base + off, 16)
        if r.get("ok"):
            words = hex_words(r["hex"])
            note = ""
            # Annotate G_DL commands and other interesting opcodes
            w0 = int(words[0], 16)
            w1 = int(words[1], 16)
            op = (w0 >> 24) & 0xFF
            if op == 0xDE:
                target = w1
                note = f"   <-- G_DL push={(w0 >> 16) & 0xFF:#04x} target=0x{target:08X}"
            elif op == 0xDF:
                note = "   <-- G_ENDDL"
            elif op == 0xFD:
                note = "   <-- G_SETTIMG"
            elif op == 0xFB:
                note = "   <-- G_SETENVCOLOR"
            elif op == 0x01:
                note = "   <-- G_VTX"
            print(f"  0x{base+off:08X}: {' '.join(words)}{note}")


# Around the bad parent
dump_window("around 0x801A0390 (bad parent G_DL)", 0x801A0300, 0x100)

# Forward into 0x801A04xx
dump_window("0x801A0400+ (continuation)", 0x801A0400, 0x60)

# Read at 0x80192xxx where the GOOD DL chain was
dump_window("around 0x80192000 (start of the OK DL chain)", 0x80192000, 0x80)

# Find 0x80208F20 itself — what might be there?
dump_window("the bad target 0x80208F20 (audio?)", 0x80208F00, 0x40)

# Stadium's frame buffer locations — Stadium's video manager allocates these
# Let's try common addresses
print("\n=== check segments / segment_table area ===")
# Stadium sets gSegments[0..15] somewhere. Try 0x80075000+ for game state vars
for addr in (0x80075660, 0x800756A0, 0x800756E0):
    r = peek(addr, 16)
    if r.get("ok"):
        print(f"  0x{addr:08X}: {' '.join(hex_words(r['hex']))}")
