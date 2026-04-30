"""Phase 1 audio diagnosis: drive intro->title->area-select then dump
interesting_fns + resolver_log so we can answer:
  1. Did n_alInit / n_alSynNew / func_80037340 run?
  2. Which sound-bank parser path fired (func_80039D58 / _F28 / _A10C)?
  3. Did func_8003BD2C (SoundBank loader) ever run?
  4. Did func_8003C204 ever resolve a struct whose layout matches the
     unk_D_800FC7D0_08C that crashes at unk_2C?

The resolver_log gives us (arr, base, count) for each call. By
matching `arr` against `base` we can infer which struct fields got
fixed up. If we see a call with count==N where the array is at an
offset matching unk_2C of any captured base, that field IS being
resolved. If not, hypothesis confirmed: unk_2C never gets resolved.
"""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_phase1.txt", "w")
def w(s): OUT.write(s + "\n"); OUT.flush(); print(s)

def call(c, t=3):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    s.sendall((c + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        x = s.recv(65536)
        if not x: break
        buf += x
    s.close()
    return json.loads(buf.decode().strip())

call(json.dumps({"cmd":"claim_input"}))
for _ in range(40):
    s = call("status")
    if s.get("frame", 0) >= 120: break
    time.sleep(0.3)
w(f"booted at frame {s.get('frame')}")

def state():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")

w(f"init state: {state()}")

# Snapshot resolver+counters BEFORE first press (boot-time only)
w("\n=== boot-time snapshot ===")
r = call("interesting_fns")
boot_fns = {f['name']: f['count'] for f in r.get("fns", [])}
r = call("resolver_log")
boot_resolver = r.get("entries", [])
w(f"boot resolver_log entries: {len(boot_resolver)}")
for e in boot_resolver[:32]:
    w(f"  arr={e['arr']} base={e['base']} count={e['count']}")
w("boot interesting_fns (audio only):")
for n in sorted(boot_fns):
    if any(k in n for k in ("alInit","alSynNew","alAudioFrame","8003","80037","80039","8004")):
        w(f"  {n}: {boot_fns[n]}")

# Press 1: intro -> title
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(1.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(2.0)
w(f"\nafter press1+wait: {state()}")

# Press 2: title -> area-select
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(1.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(0.5)
w(f"after press2+wait: {state()}")

# Final snapshot — full interesting_fns + full resolver_log
w("\n=== final snapshot (post-AREA_SELECT) ===")
r = call("interesting_fns")
final_fns = {f['name']: f['count'] for f in r.get("fns", [])}
r = call("resolver_log")
final_resolver = r.get("entries", [])
w(f"final resolver_log entries: {len(final_resolver)} "
  f"(boot was {len(boot_resolver)})")
# Show only the new entries since boot
for e in final_resolver:
    w(f"  arr={e['arr']} base={e['base']} count={e['count']}")

w("\nfinal interesting_fns (all):")
for n in sorted(final_fns):
    cnt = final_fns[n]
    delta = cnt - boot_fns.get(n, 0)
    flag = " <-- NEW" if delta and boot_fns.get(n, 0) == 0 else ""
    w(f"  {n}: {cnt} (boot:{boot_fns.get(n,0)}, delta:+{delta}){flag}")

OUT.close()
