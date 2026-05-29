"""Boot, turbo, drive Start to attract, wait for send_dl freeze, dump post-mortem."""
import json, socket, time, sys

def call(cmd, t=5.0):
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

# Wait for boot
print("[wait for boot]")
deadline = time.time() + 30
while time.time() < deadline:
    try:
        s = call({"cmd":"status"}, t=1.0)
        if s.get("frame", 0) >= 60: break
    except Exception:
        time.sleep(0.5); continue
    time.sleep(0.5)
print(f"booted, frame={s.get('frame')}")

# Turbo on, claim, Start x2
call({"cmd":"fast_forward","on":True}); print("[turbo on]")
call({"cmd":"claim_input"})
for i in range(2):
    call({"cmd":"set_button","name":"Start","down":True}); time.sleep(0.1)
    call({"cmd":"set_button","name":"Start","down":False}); time.sleep(0.6)

# Wait for freeze
print("[wait for send_dl freeze]")
last_dl = -1; stuck_since = None
deadline = time.time() + 60
while time.time() < deadline:
    s = call({"cmd":"status"})
    dl = s.get("send_dl")
    if dl != last_dl:
        last_dl = dl; stuck_since = time.time()
    elif time.time() - stuck_since > 3.0:
        print(f"FROZEN at send_dl={dl} frame={s.get('frame')}")
        break
    time.sleep(0.5)

# Trigger on-demand post-mortem
print("[post_mortem_dump]")
print(call({"cmd":"post_mortem_dump"}))

# Print interesting bits
import json as _json
report = _json.loads(open("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json").read())
print("\nreason:", report.get("reason"))
print("status:", _json.dumps(report.get("status"), indent=2))
print("\nthreads:")
for t in report.get("threads", []):
    name = t.get("name") or "?"
    print(f"  tid={t['tid']:6} name='{name}' frames={len(t.get('frames',[]))}")
    # Print top 6 frames
    for fr in t.get("frames", [])[:6]:
        print(f"    #{fr['i']:02} {fr['name']} ({fr['file']}:{fr['line']})")
