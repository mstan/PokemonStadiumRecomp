"""Press twice (intro->title->area-select) and check interesting fns."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_two.txt", "w")
def w(s): OUT.write(s + "\n"); OUT.flush()

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
w("booted at " + str(s.get("frame")))

def state():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")

w(f"init state: {state()}")

# Press 1: intro -> title
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(1.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(2.0)
w(f"after press1+wait: {state()}")

# Press 2: title -> area-select
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(1.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))

# Sample state at high freq during press 2 hold + after
for i in range(20):
    time.sleep(0.1)
    w(f"  +{100*i}ms: state={state()}")

w("===final interesting_fns:===")
r = call("interesting_fns")
for f in r.get("fns", []):
    w(f"  {f['name']}: {f['count']}")
OUT.close()
