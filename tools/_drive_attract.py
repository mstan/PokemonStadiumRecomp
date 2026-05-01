"""Press Start twice to reach title, then sample status repeatedly to characterize attract softlock."""
import json, socket, time, sys

def call(cmd, t=2.0):
    try:
        s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    except OSError as e:
        return {"_err": str(e)}
    try:
        s.sendall((json.dumps(cmd)+"\n").encode())
        s.settimeout(t); buf=b""
        while True:
            try: c=s.recv(65536)
            except socket.timeout: break
            if not c: break
            buf += c
            if buf.endswith(b"\n"): break
    finally: s.close()
    try: return json.loads(buf.decode(errors="replace").strip())
    except: return {"_err": "parse"}

print("claim:", call({"cmd":"claim_input"}))
time.sleep(0.5)

for i in range(2):
    call({"cmd":"set_button","name":"Start","down":True}); time.sleep(0.1)
    call({"cmd":"set_button","name":"Start","down":False})
    time.sleep(2.0)
    st = call({"cmd":"status"})
    print(f"after Start#{i+1}: frame={st.get('frame')} send_dl={st.get('send_dl')}")

print()
print("Idle 25s — attract should auto-trigger after ~15s on title.")
print("t   frame   send_dl  pc_trail(last4)        watchdog")
t0 = time.time()
for i in range(30):
    st = call({"cmd":"status"})
    pc = call({"cmd":"get_last_pc_trail"})
    pct = pc.get("pc_trail", [])[-4:] if isinstance(pc, dict) else []
    wd = pc.get("watchdog_count") if isinstance(pc, dict) else "?"
    print(f"{time.time()-t0:5.1f}  {st.get('frame'):5}  {st.get('send_dl'):5}  {pct}  wd={wd}")
    time.sleep(1.0)
