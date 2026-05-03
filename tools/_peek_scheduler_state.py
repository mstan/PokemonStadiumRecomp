"""Peek the task scheduler state machines.
gfx struct lives at 0x80037540 (task at +0x20=0x80037560)
audio struct lives at 0x8005AA90 (task at +0x20=0x8005AAB0)
unk_1C: 0=idle, 1=running, 2=yielded, 3=complete
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

slots = [
    ("GFX",    0x80037540),
    ("AUDIO",  0x8005AA90),
]

for name, base in slots:
    # Read unk_1C (u16 at base+0x1C) and unk_1E (u16 at base+0x1E) and task.t.type (u32 at base+0x20)
    r = call({"cmd": "rdram_peek", "addr": base + 0x1C, "n": 4})  # gets 4 bytes
    raw = bytes.fromhex(r["hex"])
    unk_1C = int.from_bytes(raw[0:2], "big")
    unk_1E = int.from_bytes(raw[2:4], "big")
    rt = call({"cmd": "rdram_peek", "addr": base + 0x20, "n": 4})  # OSTask.t.type
    task_type = int.from_bytes(bytes.fromhex(rt["hex"]), "big")
    state_name = {0: "IDLE", 1: "RUNNING", 2: "YIELDED", 3: "COMPLETE"}.get(unk_1C, "?")
    type_name = {1: "M_GFXTASK", 2: "M_AUDTASK", 3: "M_VIDTASK", 4: "M_NJPEGTASK"}.get(task_type, f"type{task_type}")
    print(f"{name:6}  base={base:#010x}  unk_1C={unk_1C} ({state_name})  unk_1E={unk_1E}  task.type={task_type} ({type_name})")
