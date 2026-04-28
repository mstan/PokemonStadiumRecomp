"""Read the OSMesg[] buffer of D_800A6CD0 to see what messages are ringing through."""
import socket, json, struct
HOST, PORT = "127.0.0.1", 4371

def call(cmd):
    s = socket.create_connection((HOST, PORT), timeout=4); s.sendall((cmd+"\n").encode()); s.settimeout(3)
    buf=b""; import time; end=time.time()+3
    while time.time()<end:
        try:
            c=s.recv(65536)
            if not c: break
            buf+=c
            if buf.endswith(b"\n"): break
        except: break
    s.close()
    return json.loads(buf.decode(errors="replace").strip())

def peek(addr, n):
    r=call(f'{{"cmd":"rdram_peek","addr":{addr},"n":{n}}}')
    raw=(r.get("hex") or r.get("data") or "").replace(" ","")
    return bytes.fromhex(raw) if raw else b""

# OSMesg[] of D_800A6CD0 lives at 0x800A6C90, cap=16, 4 bytes per OSMesg
b = peek(0x800A6C90, 64)
print(f"D_800A6CD0 msg[] @ 0x800A6C90 (16 entries, 4B each):")
for i in range(16):
    v = struct.unpack(">I", b[i*4:i*4+4])[0]
    print(f"  msg[{i:2}] = 0x{v:08x}  (decimal {v})")

# OSMesgQueue D_800A6CD0 itself
q = peek(0x800A6CD0, 0x18)
mt    = struct.unpack(">I", q[0x00:0x04])[0]
full  = struct.unpack(">I", q[0x04:0x08])[0]
valid = struct.unpack(">i", q[0x08:0x0C])[0]
first = struct.unpack(">i", q[0x0C:0x10])[0]
cap   = struct.unpack(">i", q[0x10:0x14])[0]
msgp  = struct.unpack(">I", q[0x14:0x18])[0]
print(f"\nQueue header:")
print(f"  mtqueue={mt:#x}  fullqueue={full:#x}  validCount={valid}  first={first}  cap={cap}  msg=0x{msgp:08x}")
