"""Try different directional sequences from AREA_SELECT (state 0x10)
to find the Kids Club / minigames entrance."""
import socket, json, time, sys
from pathlib import Path

ERRLOG = Path("F:/Projects/PokemonStadiumRecomp/build/last_error.log")

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

def boot_to_title():
    call(json.dumps({"cmd":"claim_input"}))
    for _ in range(40):
        s = call("status")
        if s.get("frame", 0) >= 120: break
        time.sleep(0.3)
    print(f"booted state={state()}")
    press("Start", 200); time.sleep(2.0)
    print(f"after S1: state={state()}")
    press("Start", 200); time.sleep(2.0)
    print(f"after S2: state={state()} (expect 0x04 MENU_SELECT)")

def try_seq(label, seq):
    print(f"\n=== {label} ===")
    for tag in seq:
        if crashed(): print(f"!!! crash before {tag}"); return
        if tag.startswith("wait"):
            time.sleep(float(tag.split(":")[1])); continue
        press(tag, 150)
        time.sleep(2.0)
        print(f"  after {tag}: state={state()}")
        if crashed(): print(f"!!! CRASHED at {tag}"); return

boot_to_title()
# State 4 is MENU_SELECT. Try: down then A — see what menu items are
try_seq("press A only (default highlight)", ["A","A","A"])
