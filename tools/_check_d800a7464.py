"""Read D_800A7464 -> {.unk_16, .unk_18[3]} to see if the per-frame DL
pointers are valid or stale."""
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


# D_800A7464 is a global pointer
print("=== D_800A7464 (global pointer) at 0x800A7464 ===")
r = peek(0x800A7464, 4)
struct_ptr = int(r["hex"], 16) if r.get("ok") else 0
print(f"  -> 0x{struct_ptr:08X}")

if struct_ptr == 0:
    print("  pointer is NULL — no struct allocated yet")
    raise SystemExit(0)

# Read the struct (size 0x24)
print(f"\n=== struct at 0x{struct_ptr:08X} (size 0x24) ===")
r = peek(struct_ptr, 0x24)
if r.get("ok"):
    h = r["hex"]
    print(f"  raw hex: {h}")
    unk_16 = int(h[0x16*2:0x16*2+2], 16)
    print(f"  unk_16 (current frame index) = {unk_16}")
    p18 = [int(h[(0x18 + i*4)*2:(0x18 + i*4)*2 + 8], 16) for i in range(3)]
    print(f"  unk_18[0] = 0x{p18[0]:08X}")
    print(f"  unk_18[1] = 0x{p18[1]:08X}")
    print(f"  unk_18[2] = 0x{p18[2]:08X}")

    print(f"\n=== contents at each unk_18[i] (32 bytes) ===")
    for i, ptr in enumerate(p18):
        if 0x80000000 <= ptr < 0x80800000:
            r2 = peek(ptr, 32)
            if r2.get("ok"):
                words = hex_words(r2["hex"])
                first_byte = int(r2["hex"][:2], 16)
                op_names = {0xDE: "G_DL", 0xDF: "G_ENDDL", 0xE7: "G_RDPHALF",
                            0xE3: "G_SETOTHERMODE_H", 0xFB: "G_SETENVCOLOR",
                            0xDB: "G_MOVEWORD", 0xFD: "G_SETTIMG", 0xDA: "G_MTX",
                            0xD7: "G_TEXTURE", 0xD9: "G_GEOMETRYMODE",
                            0xDC: "G_MOVEMEM", 0x01: "G_VTX"}
                hint = op_names.get(first_byte, f"0x{first_byte:02X}?")
                marker = "  <-- audio?" if 0x80200000 <= ptr < 0x80300000 else ""
                print(f"  unk_18[{i}] = 0x{ptr:08X} (op {hint:>20}): {' '.join(words)}{marker}")
        else:
            print(f"  unk_18[{i}] = 0x{ptr:08X}  (out of range)")

print(f"\n=== gDisplayListHead at 0x800A7420 ===")
r = peek(0x800A7420, 4)
if r.get("ok"):
    print(f"  -> 0x{int(r['hex'], 16):08X}")

print(f"\n=== context 0x800A7400-0x800A7480 ===")
r = peek(0x800A7400, 0x80)
if r.get("ok"):
    h = r["hex"]
    for i in range(0, len(h), 32):
        line = h[i:i+32]
        print(f"  0x{0x800A7400 + i//2:08X}: {' '.join(hex_words(line))}")
