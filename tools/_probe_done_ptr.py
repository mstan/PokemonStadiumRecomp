"""Peek the pointer at 0x800A7464 + offset 0x11 to see what func_80007604 reads."""
import socket, json, time, sys

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
print(f"booted at {s.get('frame')}")

# Press once to advance to title (so the func_80007604 ptr is initialized)
call(json.dumps({"cmd":"set_button","name":"Start","down":True}))
time.sleep(1.5)
call(json.dumps({"cmd":"set_button","name":"Start","down":False}))
time.sleep(2)

ptr_addr = 0x800A7464
ptr_word = call(json.dumps({"cmd":"rdram_peek","addr":ptr_addr,"n":4})).get("hex","?")
print(f"pointer @ 0x{ptr_addr:08X} = {ptr_word}")

ptr_val = int(ptr_word, 16)
if ptr_val != 0:
    target = ptr_val + 0x11
    byte_val = call(json.dumps({"cmd":"rdram_peek","addr":target,"n":4})).get("hex","?")
    print(f"  *ptr+0x11 = {byte_val}")
    wide = call(json.dumps({"cmd":"rdram_peek","addr":ptr_val,"n":32})).get("hex","?")
    print(f"  ptr struct (32 bytes): {wide}")
