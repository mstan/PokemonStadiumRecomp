"""Probe each unique G_DL target in the failing DL. Identify which look
like valid sub-DLs vs which contain garbage (e.g. audio data)."""
import json, socket, struct
from collections import Counter

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


VALID_OPS = {0x00, 0x01, 0x03, 0x05, 0x06,
             0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
             0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
             0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
             0xF0, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9,
             0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF}


def looks_like_dl(hex_bytes):
    valid = 0
    for i in range(4):
        op = int(hex_bytes[i*16:i*16+2], 16)
        if op in VALID_OPS:
            valid += 1
    return valid >= 3


data = open("build/failing_dl.bin", "rb").read()
DL_START = 0x80191FC0

targets = Counter()
for off in range(0, len(data) - 8, 8):
    w0, w1 = struct.unpack(">II", data[off:off+8])
    if (w0 >> 24) == 0xDE:
        targets[w1] += 1

print(f"Total unique G_DL targets: {len(targets)}")
print()
print("=== Probing each target's first 32 bytes in live rdram ===")
broken_targets = []
all_targets = sorted(targets.items())
for t, c in all_targets:
    if t < 0x80000000 or t >= 0x80800000:
        continue
    r = peek(t, 32)
    if not r.get("ok"):
        continue
    h = r["hex"]
    if not looks_like_dl(h):
        broken_targets.append((t, c, h))

print(f"\nValid: {len(all_targets) - len(broken_targets)} / Broken: {len(broken_targets)}")
print()
print("=== Broken targets ===")
for t, c, h in broken_targets:
    words = [h[i:i+8] for i in range(0, len(h), 8)]
    first_op = int(h[:2], 16)
    print(f"  0x{t:08X} ({c}x): first_op=0x{first_op:02X}  {' '.join(words)}")
