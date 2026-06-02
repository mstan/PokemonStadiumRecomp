"""Decode gdl_walk ring around submit_seq 3608 (the hung submission)."""
import json, socket

r = json.load(open('build/last_run_report.json'))

# Read live rdram for the targets we identify
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


print("=== gdl_submit (last 5) ===")
g = r['gdl_submit']
events = g.get('events') if isinstance(g, dict) else g
for e in events[-5:]:
    print(' ', e)

print()
print("=== gdl_walk submit_seq=3608 events ===")
g = r['gdl_walk']
events = g.get('events') if isinstance(g, dict) else g
seq3608 = [e for e in events if e.get('submit_seq') == 3608]
print(f"  total events for seq=3608: {len(seq3608)}")
print(f"  last 12 events for seq=3608:")
for e in seq3608[-12:]:
    h = e['head']
    # decode first 8 bytes as Gfx command
    w0 = int(h[0:8], 16)
    w1 = int(h[8:16], 16)
    op = (w0 >> 24) & 0xFF
    print(f"    target=0x{e['target']:08X} parent=0x{e['parent']:08X} "
          f"w0=0x{w0:08X} w1=0x{w1:08X} op=0x{op:02X} head={h[:32]}")

print()
# The LAST entry's target (0x80294B60) is where it broke. Read that region:
LAST_TARGET = 0x80294B60
print(f"=== Live rdram peek 0x{LAST_TARGET:08X} (128 bytes) ===")
r2 = peek(LAST_TARGET, 128)
if r2.get("ok"):
    h = r2["hex"]
    for i in range(0, len(h), 32):
        line = h[i:i+32]
        words = [line[j:j+8] for j in range(0, len(line), 8)]
        print(f"  0x{LAST_TARGET + i//2:08X}: {' '.join(words)}")

# Also check what the parent of that target was
PARENT = 0x80294A50  # from a recent walk; let's pick one
# from the events above — find parent for the last target
last_e = seq3608[-1] if seq3608 else None
if last_e:
    parent = last_e['parent']
    print()
    print(f"=== Parent slot 0x{parent:08X} (32 bytes — the cmd that pointed here) ===")
    r3 = peek(parent - 8, 32)
    if r3.get("ok"):
        h = r3["hex"]
        for i in range(0, len(h), 32):
            line = h[i:i+32]
            words = [line[j:j+8] for j in range(0, len(line), 8)]
            print(f"  0x{parent - 8 + i//2:08X}: {' '.join(words)}")
