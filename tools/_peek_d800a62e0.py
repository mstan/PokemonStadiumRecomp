"""Peek the scheduler thread struct D_800A62E0 to find live slot pointers."""
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

base = 0x800A62E0
# Slot pointers at offsets 0xA14, 0xA18, 0xA1C, 0xA20, 0xA24
for off, name in [(0xA10, "unk_A10"), (0xA14, "slot_A14"), (0xA18, "slot_A18"),
                  (0xA1C, "slot_A1C"), (0xA20, "slot_A20"), (0xA24, "slot_A24"),
                  (0xA28, "unk_A28"), (0xA38, "unk_A38(s16)")]:
    r = call({"cmd": "rdram_peek", "addr": base + off, "n": 4})
    raw = bytes.fromhex(r["hex"])
    val = int.from_bytes(raw, "big")
    print(f"{name:14}  @{base+off:#010x}  = {val:#010x}")

print()
print("# Read each slot's unk_1C state if pointer is non-null")
for off in [0xA14, 0xA18, 0xA1C, 0xA20, 0xA24]:
    r = call({"cmd": "rdram_peek", "addr": base + off, "n": 4})
    ptr = int.from_bytes(bytes.fromhex(r["hex"]), "big")
    if ptr == 0 or ptr < 0x80000000 or ptr > 0x80800000:
        print(f"  slot@{base+off:#010x} = {ptr:#010x}  [SKIP — out of RDRAM]")
        continue
    # Read u16 unk_1C, u16 unk_1E
    r2 = call({"cmd": "rdram_peek", "addr": ptr + 0x1C, "n": 4})
    raw = bytes.fromhex(r2["hex"])
    unk_1C = int.from_bytes(raw[0:2], "big")
    unk_1E = int.from_bytes(raw[2:4], "big")
    # task.t.type at +0x20
    rt = call({"cmd": "rdram_peek", "addr": ptr + 0x20, "n": 4})
    task_type = int.from_bytes(bytes.fromhex(rt["hex"]), "big")
    state_name = {0: "IDLE", 1: "RUNNING", 2: "YIELDED", 3: "COMPLETE"}.get(unk_1C, "?")
    type_name = {1: "M_GFXTASK", 2: "M_AUDTASK", 3: "M_VIDTASK", 4: "M_NJPEGTASK"}.get(task_type, f"type{task_type}")
    print(f"  slot@{base+off:#010x} -> {ptr:#010x}  unk_1C={unk_1C} ({state_name})  unk_1E={unk_1E}  task.type={task_type} ({type_name})")
