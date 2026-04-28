"""Read OSMesgQueue.validCount/first/msgCount for the gfx scheduler queues."""
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

# OSMesgQueue layout (libultra):
#  +0x00 OSThread* mtqueue  (writers waiting because queue full)
#  +0x04 OSThread* fullqueue (readers waiting because queue empty)
#  +0x08 s32      validCount
#  +0x0C s32      first
#  +0x10 s32      msgCount
#  +0x14 OSMesg*  msg
def read_queue(addr, name):
    b = peek(addr, 0x18)
    if len(b) < 0x18:
        print(f"  {name} @ 0x{addr:08x}: short read")
        return
    mt    = struct.unpack(">I", b[0x00:0x04])[0]
    full  = struct.unpack(">I", b[0x04:0x08])[0]
    valid = struct.unpack(">i", b[0x08:0x0C])[0]
    first = struct.unpack(">i", b[0x0C:0x10])[0]
    cap   = struct.unpack(">i", b[0x10:0x14])[0]
    msgp  = struct.unpack(">I", b[0x14:0x18])[0]
    print(f"  {name} @ 0x{addr:08x}: validCount={valid}/{cap}  first={first}  "
          f"mtqueue=0x{mt:08x}  fullqueue=0x{full:08x}  msg=0x{msgp:08x}")

print("=== Stadium gfx scheduler queues ===")
read_queue(0x8008468C, "D_8008468C (game->t5: frame request)")
read_queue(0x800846A4, "D_800846A4 (t5->game: 'DONE')   ")
print()
print("=== Audio scheduler queues ===")
read_queue(0x800a8640, "D_800A8640 (audio internal)     ")
read_queue(0x800a83ac, "D_800A83AC (SP DONE for ucode A)")
read_queue(0x800a8414, "D_800A8414 (SP DONE for ucode B)")
read_queue(0x80083e60, "D_80083E60 (sched dispatch)     ")
read_queue(0x800a6cd0, "D_800A6CD0 (audio main wait)    ")
print()
print("=== thread20_rsp (id=20) related queue (SP/SI events?) ===")
read_queue(0x800a6074, "D_800A6074                       ")
read_queue(0x800a62c0, "D_800A62C0                       ")
read_queue(0x800a7e70, "D_800A7E70                       ")
read_queue(0x80083c04, "D_80083C04                       ")
read_queue(0x80083bec, "D_80083BEC                       ")
