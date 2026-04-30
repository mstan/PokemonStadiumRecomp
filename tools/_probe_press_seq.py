"""Press 1, release, press 2 sequence with peeks between each."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_seq.txt", "w")
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
w(f"booted at {s.get('frame')}")

def peek_cont():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x800A7320,"n":12})).get("hex","?")
def peek_pads():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x800A73E0,"n":4})).get("hex","?")
def peek_state():
    return call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4})).get("hex","?")

w(f"INIT: state={peek_state()} cont={peek_cont()} pads={peek_pads()}")

# Press 1 (advances intro -> title)
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(0.3)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
w(f"after press1 release: state={peek_state()} cont={peek_cont()} pads={peek_pads()}")
time.sleep(0.5)
w(f"+0.5s: state={peek_state()} cont={peek_cont()} pads={peek_pads()}")
time.sleep(1.0)
w(f"+1.5s: state={peek_state()} cont={peek_cont()} pads={peek_pads()}")
time.sleep(1.0)
w(f"+2.5s: state={peek_state()} cont={peek_cont()} pads={peek_pads()}")

# Now press 2
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
w(f"press2 down: pads={peek_pads()}")
for i in range(20):
    time.sleep(0.05)
    w(f" t={50*i}ms: state={peek_state()} cont={peek_cont()} pads={peek_pads()}")
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(1)
w(f"after press2 release: state={peek_state()} cont={peek_cont()}")
OUT.close()
