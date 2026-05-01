"""After timeline run, attach to a still-running runner and dump comprehensive state."""
import json, socket, time

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

print("=== status ===")
print(json.dumps(call({"cmd":"status"}), indent=2))

print()
print("=== last_pc_trail (RSP) ===")
print(call({"cmd":"get_last_pc_trail"}).get("pc_trail",[])[-8:])

# Sample status 5 times over 5 sec to confirm freeze
print()
print("=== 5-sec sample (frame, send_dl, submit_gfx, submit_audio) ===")
for i in range(5):
    s = call({"cmd":"status"})
    print(f"  frame={s.get('frame'):5} send_dl={s.get('send_dl')} submit_gfx={s.get('submit_gfx')} submit_audio={s.get('submit_audio')}")
    time.sleep(1.0)

# Check D_8438E79C and D_8438E7AC at runtime addresses
# fragment62 link=0x84300000 runtime=0x80123360
FRAG62_LINK = 0x84300000
FRAG62_RUN = 0x80123360
def to_run(link): return FRAG62_RUN + (link - FRAG62_LINK)

print()
print("=== loop exit / scheduler state ===")
# D_800A7464 = 0x800A7464 — points to scheduler struct (shown earlier as 0x80229510)
r = call({"cmd":"rdram_peek","addr":0x800A7464,"n":4})
ptr = int(r["hex"], 16) if r.get("ok") else 0
print(f"  D_800A7464 -> 0x{ptr:08X}")
if ptr >= 0x80000000 and ptr < 0x80800000:
    rs = call({"cmd":"rdram_peek","addr":ptr,"n":24})
    h = rs.get("hex","")
    print(f"  *D_800A7464 [24 bytes]: {h}")
    if len(h) >= 36:
        unk_11 = int(h[34:36], 16)
        unk_0C = int(h[24:32], 16)
        print(f"    -> unk_0C=0x{unk_0C:08X}  unk_11=0x{unk_11:02X}  (loop exits when unk_11==1 AND var_s2!=0)")
gs = call({"cmd":"rdram_peek","addr":0x80075668,"n":4})
print(f"  gCurrentGameState (0x80075668): {gs.get('hex')}")
gs_last = call({"cmd":"rdram_peek","addr":0x80075670,"n":4})
print(f"  D_80075670 (gLastGameState?): {gs_last.get('hex')}")

print()
print("=== fragment62 attract state ===")
for name, link, n in [
    ("D_8438E79C (mode)", 0x8438E79C, 4),
    ("D_8438E7A0 (sub)", 0x8438E7A0, 4),
    ("D_8438E7AC (exit ctr)", 0x8438E7AC, 4),
    ("D_8438E440 (arg1 hdr 16)", 0x8438E440, 16),
]:
    rt = to_run(link)
    r = call({"cmd":"rdram_peek","addr":rt,"n":n})
    print(f"  {name:24}  link=0x{link:08X} run=0x{rt:08X}: {r.get('hex') if r.get('ok') else r}")

# Look at game-thread function invocations in trace ring
print()
print("=== trace_recent (last 200) — top counts ===")
tr = call({"cmd":"trace_recent","n":200})
from collections import Counter
c = Counter(tr.get("entries", []))
targets_observed = {fn for fn in tr.get("entries",[]) if fn.startswith("func_843") or fn in ("func_843010EC","func_84300E88","process_geo_layout","func_84301094","func_8001C07C","func_8001F730","Cont_StartReadInputs")}
print(f"  game-thread render targets seen: {targets_observed}")
print("  top 12 functions in last 200:")
for fn, cnt in c.most_common(12):
    print(f"    {cnt:4} {fn}")

# mesg activity since freeze: peek mesg_recent and see if game thread is recv'ing/sending
print()
print("=== mesg_recent (last 50, op summary) ===")
mr = call({"cmd":"mesg_recent","n":50})
op_counts = Counter(e["op"] for e in mr.get("events",[]))
print(f"  op counts in last 50: {dict(op_counts)}")
mq_counts = Counter(e["mq"] for e in mr.get("events",[]))
print(f"  per-mq: {dict(mq_counts)}")
