"""Drive Start*2 then A repeatedly looking for Ekans minigame crash.

Per the user, no directionals are needed — pressing A advances on
default highlights into a minigame.
"""
import socket, json, time, sys
from pathlib import Path

ERRLOG = Path("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_error.log")

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

def press(name, ms=150):
    call(json.dumps({"cmd":"set_button","name":name,"down":True}))
    time.sleep(ms/1000.0)
    call(json.dumps({"cmd":"set_button","name":name,"down":False}))

def state():
    r = call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4}))
    return r.get("hex","?")

def crashed():
    return ERRLOG.exists()

call(json.dumps({"cmd":"claim_input"}))
for _ in range(40):
    s = call("status")
    if s.get("frame", 0) >= 120: break
    time.sleep(0.3)
print(f"booted at frame {s.get('frame')} state={state()}")

# Press 1 + 2 (intro -> title -> area_select)
press("Start", 200); time.sleep(2.0)
print(f"after Start#1: state={state()}")
press("Start", 200); time.sleep(2.0)
print(f"after Start#2: state={state()}")

# Now try pressing A repeatedly
for i in range(15):
    if crashed():
        print(f"!!! CRASHED after {i} A presses")
        break
    press("A", 150)
    time.sleep(2.0)
    s = state()
    print(f"after A#{i+1}: state={s}")
    if crashed():
        print(f"!!! CRASHED right after A#{i+1}")
        break

print(f"\nfinal state={state()}, crashed={crashed()}")
