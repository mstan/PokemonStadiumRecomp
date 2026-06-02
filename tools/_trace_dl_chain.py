"""Trace the bad G_DL chain. Find which Stadium memory regions the
display list pointers came from.
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


# Trigger fresh dump first
s = socket.create_connection(ADDR, timeout=30)
s.sendall(b'{"cmd":"post_mortem_dump"}\n')
buf = b''
while True:
    c = s.recv(65536)
    if not c: break
    buf += c
    if buf.endswith(b'\n'): break
s.close()
print("dump:", buf.decode().strip())

r = json.load(open('build/last_run_report.json'))

# Find the bad submit's data_ptr
print()
print("=== last 5 sp_task M_GFXTASK events (need data_ptr/size for hung submission) ===")
sp = r.get('sp_task_recent') or r.get('sp_task') or {}
events = sp.get('events') if isinstance(sp, dict) else sp
gfx = [e for e in events if e.get('type') == 'M_GFXTASK']
for e in gfx[-3:]:
    print(' ', e)
print(f"  total gfx events in ring: {len(gfx)}")

# Find ALL gdl_walk events for the hung submission
print()
print("=== gdl_walk submit_seq counts (look for hung seq) ===")
gw = r['gdl_walk']
events = gw.get('events') if isinstance(gw, dict) else gw
seq_count = {}
for e in events:
    s_ = e['submit_seq']
    seq_count[s_] = seq_count.get(s_, 0) + 1
print(f"  unique submit_seqs in ring: {sorted(seq_count.keys())}")
for s_ in sorted(seq_count.keys()):
    print(f"    seq={s_}: {seq_count[s_]} walk events")

# Show last 30 events with parent decoding
print()
print("=== last 30 walk events (look for transition to bad memory) ===")
op_names = {0xDE: "G_DL", 0xDF: "G_ENDDL", 0xE7: "G_RDPHALF/PIPESYNC",
            0x01: "G_VTX", 0xE3: "G_SETOTHERMODE_H", 0x03: "G_TRI",
            0xD7: "G_TEXTURE", 0xD8: "G_POPMTX", 0xD9: "G_GEOMETRYMODE",
            0xFB: "G_SETENVCOLOR", 0xFA: "G_SETPRIMCOLOR",
            0xF5: "G_SETTILE", 0xF3: "G_LOADBLOCK", 0xFD: "G_SETTIMG"}
for e in events[-30:]:
    target = e['target']
    parent = e['parent']
    head = e['head']
    op = int(head[:2], 16)
    op_name = op_names.get(op, f"0x{op:02X}")
    flag = " <-- BAD (audio data?)" if op == 0xD1 or op == 0xD9 and head.startswith("D") else ""
    print(f"  seq={e['submit_seq']} target=0x{target:08X} parent=0x{parent:08X} op={op_name:>20} head={head[:32]}{flag}")

# Show what region 0x80208F20 / 0x80294B60 (the bad targets) live in
print()
print("=== Memory landscape ===")
landmarks = [
    (0x80000000, "exception vector"),
    (0x80000400, "entry"),
    (0x80075668, "gCurrentGameState"),
    (0x800C8810, "audio cmd buf #0 (seen in stderr)"),
    (0x800CAA10, "audio cmd buf #1"),
    (0x80100000, "1MB mark"),
    (0x80190000, "fb area?"),
    (0x80200000, "?"),
    (0x80208F20, "BAD TARGET 1"),
    (0x80294B60, "BAD TARGET 2"),
    (0x802A0000, "?"),
    (0x80300000, "?"),
    (0x80400000, "?"),
    (0x80500000, "?"),
    (0x80600000, "POOL_END_6MB"),
]
for addr, label in landmarks:
    r2 = peek(addr, 16)
    if r2.get("ok"):
        words = hex_words(r2["hex"])
        is_frag = ""
        if r2["hex"][16:32] == "465241474D454E54":
            is_frag = "  <-- FRAGMENT HEADER"
        print(f"  0x{addr:08X} {label:<35}: {' '.join(words)}{is_frag}")
