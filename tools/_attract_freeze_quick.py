"""Boot, idle until send_dl is stuck for 4 sample periods, then dump+inspect.
Quicker than _attract_no_press.py — no fixed 60s wait."""
import json, socket, time

def call(cmd, t=4.0):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    s.sendall((json.dumps(cmd) + "\n").encode())
    s.settimeout(t)
    buf = b""
    while True:
        try:
            c = s.recv(65536)
        except Exception:
            break
        if not c:
            break
        buf += c
        if buf.endswith(b"\n"):
            break
    s.close()
    return json.loads(buf.decode().strip())

# Wait for boot
print("[wait for boot]")
deadline = time.time() + 30
last_boot_status = None
while time.time() < deadline:
    try:
        s = call({"cmd": "status"}, t=1.0)
        last_boot_status = s
        if s.get("frame", 0) >= 60:
            break
    except Exception:
        time.sleep(0.5)
        continue
    time.sleep(0.5)
print(f"booted, frame={last_boot_status.get('frame') if last_boot_status else '?'}")

# Sample status every 2s; bail when stuck for 4 consecutive samples (~8s)
last_dl = None
stuck = 0
print("\nt   frame   send_dl  delta  gs")
for i in range(40):
    s = call({"cmd": "status"})
    gs = call({"cmd": "rdram_peek", "addr": 0x80075668, "n": 4})
    dl = s.get("send_dl")
    delta = (dl - last_dl) if last_dl is not None else 0
    if dl == last_dl:
        stuck += 1
    else:
        stuck = 0
    last_dl = dl
    print(f"{2*i:4}  {s.get('frame'):5}  {dl:5}  +{delta:4}   {gs.get('hex')}")
    if stuck >= 4:
        print(f"[STUCK confirmed at send_dl={dl}, dumping]")
        break
    time.sleep(2.0)

print("\n[post_mortem_dump]")
print(call({"cmd": "post_mortem_dump"}))
