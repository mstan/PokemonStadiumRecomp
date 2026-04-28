"""Aggregate libultra_recent calls by queue to spot the queue that has
RecvMesg waiters but no SendMesg producer (the blocked thread).
"""
import socket, json
from collections import Counter, defaultdict
HOST, PORT = "127.0.0.1", 4371

def call(cmd, timeout=8):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode()); s.settimeout(timeout)
    buf = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk: break
            buf += chunk
            if buf.endswith(b"\n"): break
    except socket.timeout: pass
    s.close()
    raw = buf.decode(errors="replace").strip()
    try: return json.loads(raw)
    except Exception: return {"_raw": raw}

# Pull max recent (1024)
r = call('{"cmd":"libultra_recent","n":1024}')
es = r.get("events", [])
print(f"got {len(es)} libultra events, write_idx={r.get('write_idx')}")

# Aggregate by name
print("\n=== Frequency of libultra calls in last 1024 ===")
c = Counter(e["name"] for e in es)
for n, k in c.most_common():
    print(f"  {k:>5}  {n}")

# For osRecvMesg: a0 is the queue. Count per queue.
print("\n=== osRecvMesg targets (a0=queue) ===")
recv = Counter(f"0x{e['a0']:08x}" for e in es if e["name"] == "osRecvMesg_recomp")
for q, k in recv.most_common():
    print(f"  {q}: {k} calls")

# osSendMesg: a0 is the queue.
print("\n=== osSendMesg targets (a0=queue) ===")
send = Counter(f"0x{e['a0']:08x}" for e in es if e["name"] == "osSendMesg_recomp")
for q, k in send.most_common():
    print(f"  {q}: {k} calls")

# Queues that are RECEIVED FROM but never SENT to — those waiters are stuck.
# (Produced messages might have come in earlier than the ring window though.)
print("\n=== Queues with recv but no send in window (potential stuck wait) ===")
for q in recv:
    if q not in send:
        print(f"  {q}: {recv[q]} recv calls, 0 sends")

# Now sweep libultra_boot for ALL osCreateMesgQueue events (queue addr + msg buffer),
# and for osStartThread (which thread was started where).
print("\n=== Walking libultra_boot for queue/thread setup events ===")
queues_created = []
threads_started = []
pos = 0; total = None
while True:
    rb = call(f'{{"cmd":"libultra_boot","start":{pos},"n":1024}}')
    if not rb.get("ok"): break
    if total is None: total = rb.get("count", 0)
    bes = rb.get("events", [])
    if not bes: break
    for e in bes:
        if e["name"] == "osCreateMesgQueue_recomp":
            queues_created.append(e)
        if e["name"] == "osStartThread_recomp":
            threads_started.append(e)
    pos += len(bes)
    if pos >= total: break

print(f"\n{len(queues_created)} osCreateMesgQueue (showing first 60):")
for e in queues_created[:60]:
    print(f"  q=0x{e['a0']:08x} msgbuf=0x{e['a1']:08x} count={e['a2']}")

print(f"\n{len(threads_started)} osStartThread events:")
for e in threads_started:
    print(f"  thread=0x{e['a0']:08x} ms={e['ms']}")
