"""Sweep all queues from libultra_boot, dump their state, and find which
queue holds each thread."""
import socket, json, struct
HOST, PORT = "127.0.0.1", 4371

def call(cmd, timeout=6):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode()); s.settimeout(timeout)
    buf = b""
    try:
        while True:
            c = s.recv(65536)
            if not c: break
            buf += c
            if buf.endswith(b"\n"): break
    except socket.timeout: pass
    s.close()
    try: return json.loads(buf.decode(errors="replace").strip())
    except Exception: return {"_raw": buf.decode(errors="replace").strip()}

def peek(addr, n):
    r = call(f'{{"cmd":"rdram_peek","addr":{addr},"n":{n}}}')
    raw = (r.get("hex") or r.get("data") or "").replace(" ", "")
    return bytes.fromhex(raw) if raw else b""

# Walk libultra_boot for all osCreateMesgQueue events.
queues = []
pos = 0; total = None
while True:
    rb = call(f'{{"cmd":"libultra_boot","start":{pos},"n":1024}}')
    if not rb.get("ok"): break
    if total is None: total = rb.get("count", 0)
    bes = rb.get("events", [])
    if not bes: break
    for e in bes:
        if e["name"] == "osCreateMesgQueue_recomp":
            queues.append((e["a0"], e["a2"]))
    pos += len(bes);
    if pos >= total: break

THREADS = {
    0x8007f180: "id=1 IDLE",
    0x800a74c0: "id=2 crash_screen",
    0x80081900: "id=20 thread20_rsp",
    0x800a62e0: "id=3 sched_main",
    0x800a8480: "id=4 audio (pri80)",
    0x80083ca0: "id=5 gfx_sched",
    0x800a6d20: "id=21 SoftReset",
    0x8007f730: "id=6 Game_Thread",
    0x800abe10: "id=7 worker",
}

print(f"=== Sweeping {len(queues)} queues ===")
print()
print(f"{'addr':<12} {'count':>5}/{'cap':<4}  {'mtqueue':>10}  {'fullqueue':>10}  {'msg':>10}  thread blocked")
for q, cap in queues:
    b = peek(q, 0x18)
    if len(b) < 0x18: continue
    mt    = struct.unpack(">I", b[0x00:0x04])[0]
    full  = struct.unpack(">I", b[0x04:0x08])[0]
    valid = struct.unpack(">i", b[0x08:0x0C])[0]
    msgp  = struct.unpack(">I", b[0x14:0x18])[0]
    blocked = ""
    if mt in THREADS:   blocked += f"mt={THREADS[mt]} "
    elif mt:            blocked += f"mt=0x{mt:08x} "
    if full in THREADS: blocked += f"full={THREADS[full]} "
    elif full:          blocked += f"full=0x{full:08x} "
    print(f"0x{q:08x}  {valid:>5}/{cap:<4}  0x{mt:08x}  0x{full:08x}  0x{msgp:08x}  {blocked}")

print()
print("=== Threads NOT blocked on any queue (running CPU-bound) ===")
blocked_threads = set()
for q, cap in queues:
    b = peek(q, 0x18)
    if len(b) < 0x18: continue
    mt   = struct.unpack(">I", b[0x00:0x04])[0]
    full = struct.unpack(">I", b[0x04:0x08])[0]
    if mt in THREADS: blocked_threads.add(mt)
    if full in THREADS: blocked_threads.add(full)

for tp, name in THREADS.items():
    if tp not in blocked_threads:
        print(f"  0x{tp:08x}  {name}")
