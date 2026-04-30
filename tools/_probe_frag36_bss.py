"""Peek fragment36's bss area at runtime to verify zero-init."""
import socket, json, time, sys

OUT = open(sys.argv[1] if len(sys.argv) > 1 else "build/probe_bss.txt", "w")
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

# fragment36 was registered at runtime 0x8015A4D0 in the prior runs.
# Need to learn current runtime. We can't easily query; assume same.
# Press Start to load fragment36, then peek.
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(0.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(2)

# Address 0x80068BA0 holds gPlayer1Controller pointer; let's also dump
# fragment36's text+data+rodata+bss range from the section table.
# The actual runtime base is in section_addresses[155]. Without
# direct access, infer from fragment registrations: known runtime
# of fragment36 is logged in stderr.
# Simpler: peek wide around where it's likely to land. Loader log
# shows runtime, but to keep this self-contained, peek 0x8015A4D0
# (matches earlier runs).
RUNTIME = 0x8015A4D0
w(f"assuming fragment36 runtime base = 0x{RUNTIME:08X}")
for off in (0xEA0, 0xEC0, 0xEC8, 0xED0, 0xEE0):
    addr = RUNTIME + off
    h = call(json.dumps({"cmd":"rdram_peek","addr":addr,"n":4})).get("hex","?")
    w(f"  +0x{off:03X} (= 0x{addr:08X}): {h}")

# Now peek a wide area
h = call(json.dumps({"cmd":"rdram_peek","addr":RUNTIME+0xE80,"n":80})).get("hex","?")
w(f"wide at +0xE80 80 bytes: {h}")
OUT.close()
