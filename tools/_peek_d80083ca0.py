"""Peek D_80083CA0 (Thread 5 / VI flip) state."""
import json, socket

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

base = 0x80083CA0

# Read u8/u16/u32 fields per the typedef
def u32(off):
    r = call({"cmd": "rdram_peek", "addr": base + off, "n": 4})
    return int.from_bytes(bytes.fromhex(r["hex"]), "big")

def u16(off):
    r = call({"cmd": "rdram_peek", "addr": base + off, "n": 2})
    return int.from_bytes(bytes.fromhex(r["hex"]), "big")

def u8(off):
    r = call({"cmd": "rdram_peek", "addr": base + off, "n": 1})
    return int.from_bytes(bytes.fromhex(r["hex"]), "big")

print(f"D_80083CA0 @ {base:#010x}  (Thread 5 / VI-flip per-thread state)")
fields = [
    (0xA88, 'u16', 'unk_A88'),
    (0xA8A, 'u8',  'unk_A8A (set 1 just before osRecvMesg(D_8008468C))'),
    (0xA8B, 'u8',  'unk_A8B'),
    (0xA8C, 'u8',  'unk_A8C'),
    (0xA8D, 'u8',  'unk_A8D'),
    (0xA8E, 'u8',  'unk_A8E'),
    (0xA8F, 'u8',  'unk_A8F'),
    (0xA90, 'u32', 'unk_A90 (gfx-data ptr from latest flip; 0 = no DL)'),
    (0xA94, 'u32', 'unk_A94'),
    (0xA98, 'u32', 'unk_A98'),
    (0xA9C, 'u8',  'unk_A9C'),
    (0xA9D, 'u8',  'unk_A9D'),
    (0xA9E, 'u8',  'unk_A9E'),
    (0xA9F, 'u8',  'unk_A9F'),
    (0xAA0, 'u32', 'unk_AA0 (controls line 119 wait-for-DONE)'),
    (0xAA8, 'u32', 'unk_AA8 (UnkArray4* fb)'),
    (0xAAC, 'u8',  'unk_AAC (queue-drain count loop bound)'),
    (0xAAD, 'u8',  'unk_AAD'),
    (0xAAE, 'u8',  'unk_AAE'),
    (0xAAF, 'u8',  'unk_AAF'),
    (0xAB8, 'u32', 'unk_AB8'),
]
for off, typ, name in fields:
    v = {'u8': u8, 'u16': u16, 'u32': u32}[typ](off)
    width = {'u8': 2, 'u16': 4, 'u32': 8}[typ]
    print(f"  +{off:03x} {typ:<3}  {v:#0{width+2}x} ({v})  {name}")

print()
print("[Thread 5 OSThread state] (D_80083CA0 base + 0x00 is the OSThread)")
# OSThread layout:
# 0x00 next, 0x04 priority, 0x08 queue, 0x0C tlnext, 0x10 state, 0x14 flags, 0x18 id, 0x1C fp, 0x20 context...
state = u16(0x10)
state_names = {0:"STOPPED",1:"RUNNABLE",2:"RUNNING",4:"WAITING",8:"PREEMPTED",0x10:"STOP_REQ"}
print(f"  state    = {state:#06x}  ({state_names.get(state, '?')})")
print(f"  flags    = {u16(0x14):#06x}")
print(f"  id       = {u32(0x18):#010x}")
print(f"  queue    = {u32(0x08):#010x}  (queue this thread is currently parked on)")
