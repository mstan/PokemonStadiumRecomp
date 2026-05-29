"""Drive attract; once send_dl freezes, dump mesg ring + libultra ring + status."""
import json, socket, subprocess, sys, time
from pathlib import Path

ROOT = Path("F:/Projects/n64recomp/PokemonStadiumRecomp")

def call(cmd, t=4.0):
    s=socket.create_connection(("127.0.0.1",4371),timeout=t)
    s.sendall((json.dumps(cmd)+"\n").encode())
    s.settimeout(t); buf=b""
    while True:
        try: c=s.recv(65536)
        except: break
        if not c: break
        buf+=c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode().strip())

# 1. launch
proc = subprocess.Popen(
    [str(ROOT/"build/PokemonStadiumRecomp.exe")],
    cwd=str(ROOT/"build"),
    stdout=subprocess.DEVNULL,
    stderr=open(str(ROOT/"build/timeline.stderr"), "w"),
    env={**__import__("os").environ, "PSR_VOLUME": "0"})

# 2. wait for boot
print("[wait for boot]")
deadline = time.time() + 60
while time.time() < deadline:
    try:
        s = call({"cmd":"status"}, t=1.0)
        if s.get("frame", 0) >= 60: break
    except Exception:
        time.sleep(0.5); continue
    time.sleep(0.5)
print("booted, frame=", s.get("frame"))

# 3. turbo + claim + Start x2 to advance to title
call({"cmd":"fast_forward","on":True})
print("[turbo on]")
call({"cmd":"claim_input"})
for i in range(2):
    call({"cmd":"set_button","name":"Start","down":True}); time.sleep(0.1)
    call({"cmd":"set_button","name":"Start","down":False})
    time.sleep(0.6)  # turbo: shorter wait

# 4. sample send_dl until it stalls for 3s
print("[wait for send_dl freeze]")
last_dl = -1; stuck_since = None
freeze_dl = None
deadline = time.time() + 90
while time.time() < deadline:
    s = call({"cmd":"status"})
    dl = s.get("send_dl")
    if dl != last_dl:
        last_dl = dl; stuck_since = time.time()
    elif time.time() - stuck_since > 3.0:
        freeze_dl = dl
        print(f"FROZEN at send_dl={dl} frame={s.get('frame')}")
        break
    time.sleep(0.5)

# 5. dump rings
print()
print("=== status ===")
print(json.dumps(call({"cmd":"status"}), indent=2))

print()
print("=== mesg_recent (last 256) ===")
mr = call({"cmd":"mesg_recent","n":256})
events = mr.get("events", []) if isinstance(mr, dict) else []
# Group by mq, count ops
from collections import Counter, defaultdict
op_counts = Counter()
mq_op = defaultdict(Counter)
for e in events:
    op_counts[e["op"]] += 1
    mq_op[e["mq"]][e["op"]] += 1
print("op totals:", dict(op_counts))
print("per-mq op breakdown:")
for mq, c in sorted(mq_op.items(), key=lambda x: -sum(x[1].values()))[:15]:
    print(f"  mq=0x{mq:08X}: {dict(c)}")

# Find last RECV_BLOCK and most recent context around the freeze
print()
print("=== last 30 events ===")
for e in events[-30:]:
    print(f"  seq={e['seq']:7} ms={e['ms']:7} {e['op']:18} mq=0x{e['mq']:08X} msg=0x{e['msg']:08X} vb={e['vb']} va={e['va']} block={e['block']} gt={e['gt']}")

# Save report
import json as _json
(ROOT/"build/timeline_report.json").write_text(_json.dumps({"freeze_dl":freeze_dl, "status":call({"cmd":"status"}), "mesg":mr}, indent=2))
print()
print("saved to build/timeline_report.json")

# Leave runner alive for further probes (run _freeze_dump.py in another shell).
print()
print("[runner LEFT ALIVE — pid", proc.pid, "— attach probes via TCP]")
