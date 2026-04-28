"""Read D_800846C0 (UnkStruct80001380) — the gfx task state thread 5
is waiting on. Inspect unk_1C, unk_1E, task type/ucode."""
import socket, json, struct
HOST, PORT = "127.0.0.1", 4371

def call(cmd, timeout=4):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode()); s.settimeout(timeout)
    buf = b""
    try:
        while True:
            c = s.recv(65536)
            if not c: break
            buf += c
            if buf.endswith(b"\n"): break
    except socket.timeout: pass
    s.close()
    try: return json.loads(buf.decode(errors="replace").strip())
    except Exception: return {"_raw": buf.decode(errors="replace").strip()}

def peek(addr, n):
    r = call(f'{{"cmd":"rdram_peek","addr":{addr},"n":{n}}}')
    raw = (r.get("hex") or r.get("data") or "").replace(" ", "")
    return bytes.fromhex(raw) if raw else b""

D_800846C0 = 0x800846C0
b = peek(D_800846C0, 0x68)
def u16(o): return struct.unpack(">H", b[o:o+2])[0]
def s32(o): return struct.unpack(">i", b[o:o+4])[0]
def u32(o): return struct.unpack(">I", b[o:o+4])[0]

print(f"=== D_800846C0 (UnkStruct80001380) ===")
print(f"  mesg          (+0x00) = 0x{u32(0x00):08x}")
print(f"  queue.mtq     (+0x04) = 0x{u32(0x04):08x}  (waiters waiting to recv)")
print(f"  queue.fullq   (+0x08) = 0x{u32(0x08):08x}")
print(f"  queue.valid   (+0x0C) = {s32(0x0C)}")
print(f"  queue.first   (+0x10) = {s32(0x10)}")
print(f"  queue.msgCount(+0x14) = {s32(0x14)}")
print(f"  queue.msg     (+0x18) = 0x{u32(0x18):08x}")
print(f"  unk_1C  (sp_status)   = {u16(0x1C)}  (0=init,1=running,2=yielded,3=done)")
print(f"  unk_1E  (rdp_status)  = {u16(0x1E)}  (0=init,1=pending,2=complete)")
print(f"=== OSTask (+0x20, size 0x40) ===")
print(f"  type          (+0x20) = 0x{u32(0x20):08x}  (1=GFX,2=AUD,4=other)")
print(f"  flags         (+0x24) = 0x{u32(0x24):08x}")
print(f"  ucode_boot    (+0x28) = 0x{u32(0x28):08x}")
print(f"  ucode_boot_sz (+0x2C) = 0x{u32(0x2C):08x}")
print(f"  ucode         (+0x30) = 0x{u32(0x30):08x}")
print(f"  ucode_size    (+0x34) = 0x{u32(0x34):08x}")
print(f"  ucode_data    (+0x38) = 0x{u32(0x38):08x}")
print(f"  ucode_data_sz (+0x3C) = 0x{u32(0x3C):08x}")
print(f"  dram_stack    (+0x40) = 0x{u32(0x40):08x}")
print(f"  dram_stack_sz (+0x44) = 0x{u32(0x44):08x}")
print(f"  output_buff   (+0x48) = 0x{u32(0x48):08x}")
print(f"  output_buff_sz(+0x4C) = 0x{u32(0x4C):08x}")
print(f"  data_ptr      (+0x50) = 0x{u32(0x50):08x}")
print(f"  data_size     (+0x54) = 0x{u32(0x54):08x}")
print(f"  yield_data    (+0x58) = 0x{u32(0x58):08x}")
print(f"  yield_data_sz (+0x5C) = 0x{u32(0x5C):08x}")

# Peek D_800A62E0.unk_A14 (the "currently running task" pointer for sched)
b2 = peek(0x800a62e0 + 0xA14, 4)
unk_A14 = struct.unpack(">I", b2[:4])[0] if len(b2) >= 4 else None
b3 = peek(0x800a62e0 + 0xA18, 4)
unk_A18 = struct.unpack(">I", b3[:4])[0] if len(b3) >= 4 else None
b4 = peek(0x800a62e0 + 0xA1C, 4)
unk_A1C = struct.unpack(">I", b4[:4])[0] if len(b4) >= 4 else None
b5 = peek(0x800a62e0 + 0xA38, 2)
unk_A38 = struct.unpack(">h", b5[:2])[0] if len(b5) >= 2 else None
print()
print(f"=== sched ptrs (D_800A62E0) ===")
print(f"  unk_A14 (running task) = 0x{unk_A14:08x}  {'<<< D_800846C0' if unk_A14 == D_800846C0 else ''}")
print(f"  unk_A18 (next task)    = 0x{unk_A18:08x}  {'<<< D_800846C0' if unk_A18 == D_800846C0 else ''}")
print(f"  unk_A1C (next2 task)   = 0x{unk_A1C:08x}")
print(f"  unk_A38 (mode flag)    = {unk_A38}")

# Check D_80083CA0 thread 5 state at unk_AA0 to confirm the pre-launch condition
b6 = peek(0x80083CA0 + 0xAA0, 4)
unk_AA0 = struct.unpack(">i", b6[:4])[0] if len(b6) >= 4 else None
b7 = peek(0x80083CA0 + 0xA90, 4)
unk_A90 = struct.unpack(">i", b7[:4])[0] if len(b7) >= 4 else None
print()
print(f"=== thread 5 (D_80083CA0) gfx state ===")
print(f"  unk_AA0 = {unk_AA0}  (non-zero gates the func_800049AC wait)")
print(f"  unk_A90 = {unk_A90}")
