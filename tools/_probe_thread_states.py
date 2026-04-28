"""Find all OS threads via libultra_boot's osCreateThread events,
then read OSThread.state/.flags from RDRAM via rdram_peek.

OSThread layout (libultra):
  +0x00  next *OSThread
  +0x04  priority (s32)
  +0x08  queue **OSThread
  +0x0C  tlnext *OSThread
  +0x10  state (u16)   <-- KEY FIELD
  +0x12  flags (u16)
  +0x14  id    (s32)
  +0x18  fp    (s32)
  +0x1C  __OSThreadprofile_s* (u32)
  +0x20  __OSThreadContext (200 bytes)
         +0x118 (rel) = pc (u32) -> abs offset 0x138

State enum:
  0x01 STOPPED   0x02 RUNNABLE  0x04 RUNNING  0x08 WAITING
"""
import socket, json, struct
HOST, PORT = "127.0.0.1", 4371

def call(cmd, timeout=6):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode()); s.settimeout(timeout)
    buf = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk: break
            buf += chunk
            if buf.endswith(b"\n"): break
    except socket.timeout: pass
    s.close()
    raw = buf.decode(errors="replace").strip()
    try: return json.loads(raw)
    except Exception: return {"_raw": raw}

# Walk all boot events, collect osCreateThread
print("=== Walking libultra_boot for osCreateThread events ===")
threads = []  # list of (thread_ptr, id, entry_pc, ms)
pos = 0
total = None
while True:
    r = call(f'{{"cmd":"libultra_boot","start":{pos},"n":1024}}')
    if not r.get("ok"):
        print("error:", r); break
    if total is None:
        total = r.get("count", 0)
        print(f"total boot events: {total}, capacity: {r.get('capacity')}")
    es = r.get("events", [])
    if not es: break
    for e in es:
        if e["name"] == "osCreateThread_recomp":
            threads.append((e["a0"], e["a1"], e["a2"], e["a3"], e["ms"]))
    pos += len(es)
    if pos >= total: break

print(f"\n{len(threads)} osCreateThread calls observed:")
print("  thread_ptr   id          entry_pc     sp            ms")
for tp, tid, epc, sp, ms in threads:
    print(f"  0x{tp:08x}  0x{tid:08x}  0x{epc:08x}  0x{sp:08x}  {ms}")

# Read OSThread.state/.flags/.id and context.pc for each.
def peek(addr, n):
    r = call(f'{{"cmd":"rdram_peek","addr":{addr},"n":{n}}}')
    return r

state_names = {1: "STOPPED", 2: "RUNNABLE", 4: "RUNNING", 8: "WAITING"}

print("\n=== OSThread state snapshots ===")
for tp, tid_creation, epc, sp, ms in threads:
    if tp < 0x80000000 or tp > 0x80800000:
        print(f"  thread_ptr 0x{tp:08x}: out of range, skipping")
        continue
    r = peek(tp, 0x40)
    raw = r.get("hex") or r.get("data") or r.get("_raw") or ""
    if "hex" not in r and "data" not in r:
        print(f"  thread 0x{tp:08x}: peek error: {r}")
        continue
    raw = (r.get("hex") or r.get("data") or "").replace(" ", "")
    try:
        b = bytes.fromhex(raw)
    except Exception:
        print(f"  thread 0x{tp:08x}: bad hex: {raw[:60]}")
        continue
    if len(b) < 0x20:
        print(f"  thread 0x{tp:08x}: short read len={len(b)}")
        continue
    next_p = struct.unpack(">I", b[0x00:0x04])[0]
    pri    = struct.unpack(">i", b[0x04:0x08])[0]
    state  = struct.unpack(">H", b[0x10:0x12])[0]
    flags  = struct.unpack(">H", b[0x12:0x14])[0]
    tid    = struct.unpack(">i", b[0x14:0x18])[0]
    sn     = state_names.get(state, f"?(0x{state:x})")
    # context.pc at offset 0x20 + 0x118 = 0x138 -> beyond our 0x40 read
    print(f"  thread 0x{tp:08x}: id={tid} (creation_id=0x{tid_creation:x}) pri={pri} "
          f"state=0x{state:02x}({sn}) flags=0x{flags:04x} entry=0x{epc:08x}")

# Also pull context.pc/sp/ra for any WAITING thread.
print("\n=== Context (pc/sp/ra) for each thread ===")
for tp, tid_creation, epc, sp_create, ms in threads:
    if tp < 0x80000000 or tp > 0x80800000: continue
    # Read context block (200 bytes from 0x20). pc at +0x118, sp at +0xD8, ra at +0xE8 (libultra typical)
    r = peek(tp + 0x20, 0xC8)
    raw = (r.get("hex") or r.get("data") or "").replace(" ", "")
    try: b = bytes.fromhex(raw)
    except Exception: continue
    # Try to find the pc — common libultra OSThreadContext layout puts pc at offset 0x118-0x20=0xF8 from start of context.
    # Actually correct layout: context = at,v0,v1,a0..a3,t0..t9,s0..s7,t8..t9,gp,sp,s8,ra (29 regs * 8 = 0xE8) + lo,hi (8) + pc (4)...
    # Let's just hexdump the context block — the pc is somewhere recognizable as 0x80xxxxxx.
    print(f"\n  thread 0x{tp:08x} context+0x20 (200 bytes):")
    for off in range(0, min(len(b), 0xC8), 16):
        row = b[off:off+16]
        print(f"    +0x{0x20+off:03x}  " + " ".join(f"{x:02x}" for x in row))
