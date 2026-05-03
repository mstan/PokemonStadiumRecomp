"""Peek the two flip queues:
- D_8008468C: flip-request queue (game thread -> Thread 5)
- D_800846A4: flip-done queue   (Thread 5 -> game thread)

OSMesgQueue layout (24 bytes):
  0x00 mtqueue (OSThread*)
  0x04 fullqueue (OSThread*)
  0x08 validCount (s32)
  0x0C first (s32)
  0x10 msgCount (s32)
  0x14 msg (OSMesg*)
"""
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

queues = [
    ("D_8008468C (flip-request, game->Thread5)", 0x8008468C),
    ("D_800846A4 (flip-done, Thread5->game)",    0x800846A4),
    ("D_800A62E0.queue (sched-thread queue)",    0x800A62E0 + 0x9F0),
    # GFX slot D_800846C0 has its own queue for DONE replies
    ("D_800846C0.queue (gfx-slot queue)",        0x800846C0 + 0x04),
]

for name, addr in queues:
    r = call({"cmd": "rdram_peek", "addr": addr, "n": 0x18})
    raw = bytes.fromhex(r["hex"])
    mtq    = int.from_bytes(raw[0x00:0x04], "big")
    fullq  = int.from_bytes(raw[0x04:0x08], "big")
    valid  = int.from_bytes(raw[0x08:0x0C], "big", signed=True)
    first  = int.from_bytes(raw[0x0C:0x10], "big", signed=True)
    msgcnt = int.from_bytes(raw[0x10:0x14], "big", signed=True)
    msgptr = int.from_bytes(raw[0x14:0x18], "big")
    print(f"{name}")
    print(f"  @{addr:#010x}  mtqueue={mtq:#010x}  fullqueue={fullq:#010x}  validCount={valid}  first={first}  msgCount={msgcnt}  msgptr={msgptr:#010x}")
    if mtq != 0 and mtq != 0xFFFFFFFF:
        print(f"  >>> a thread is BLOCKED waiting on this queue (mtqueue=non-null)")
    print()
