"""Boot, turbo, NO start press — just wait for attract to trigger."""
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

# Sample status every 2s, watch for send_dl freeze
print("\nt   frame   send_dl  delta  gs   submit_audio")
last_dl = None; stuck_since = None
for i in range(30):
    s = call({"cmd":"status"})
    gs = call({"cmd":"rdram_peek","addr":0x80075668,"n":4})
    dl = s.get("send_dl")
    delta = (dl - last_dl) if last_dl is not None else 0
    if dl == last_dl and stuck_since is None:
        stuck_since = i
    elif dl != last_dl:
        stuck_since = None
    last_dl = dl
    stuck_marker = " <STUCK>" if (stuck_since is not None and i - stuck_since >= 3) else ""
    print(f"{2*i:4}  {s.get('frame'):5}  {dl:5}  +{delta:4}   {gs.get('hex')}  audio={s.get('submit_audio')}{stuck_marker}")
    time.sleep(2.0)

# Dump
print("\n[post_mortem_dump]")
print(call({"cmd":"post_mortem_dump"}))
