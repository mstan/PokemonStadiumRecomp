"""Drive Start to attract+menu, then sample status every second for 15s."""
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

# Wait for boot
print("[wait for boot]")
deadline = time.time() + 30
while time.time() < deadline:
    try:
        s = call({"cmd":"status"}, t=1.0)
        if s.get("frame", 0) >= 60: break
    except: time.sleep(0.5); continue
    time.sleep(0.5)
print(f"booted, frame={s.get('frame')}")

# Turbo on (default), claim, Start x2 to advance
call({"cmd":"claim_input"})
for i in range(2):
    call({"cmd":"set_button","name":"Start","down":True}); time.sleep(0.1)
    call({"cmd":"set_button","name":"Start","down":False}); time.sleep(0.6)

# Sample status over 15s
print()
print("t   frame   send_dl   submit_gfx   submit_audio")
t0 = time.time()
last_dl, last_sg = None, None
for i in range(15):
    s = call({"cmd":"status"})
    dl = s.get("send_dl"); sg = s.get("submit_gfx")
    delta_dl = (dl - last_dl) if last_dl is not None else 0
    delta_sg = (sg - last_sg) if last_sg is not None else 0
    last_dl = dl; last_sg = sg
    print(f"{time.time()-t0:5.1f}  {s.get('frame'):5}  {dl:5} (+{delta_dl:3})  {sg:5} (+{delta_sg:3})  {s.get('submit_audio')}")
    time.sleep(1.0)
