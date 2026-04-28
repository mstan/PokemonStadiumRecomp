"""Walk gMainPool's mainState linked list — each state's preceding
MainPoolBlock holds the 4-char push tag. Order: top → root.
"""
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

GMAINPOOL = 0x800A60B0
mp = peek(GMAINPOOL, 0x40)
def u32(o): return struct.unpack(">I", mp[o:o+4])[0]
print(f"gMainPool @ 0x{GMAINPOOL:08x}:")
print(f"  available  = 0x{u32(0x1C):08x} ({u32(0x1C)} bytes free)")
print(f"  start      = 0x{u32(0x20):08x}")
print(f"  end        = 0x{u32(0x24):08x}")
print(f"  listHeadL  = 0x{u32(0x28):08x}")
print(f"  listHeadR  = 0x{u32(0x2C):08x}")
print(f"  mainState  = 0x{u32(0x30):08x}")

print("\n=== push_state stack (top → bottom) ===")
state_ptr = u32(0x30)
depth = 0
while state_ptr and 0x80000000 <= state_ptr < 0x80800000 and depth < 32:
    # Tag is in the MainPoolBlock immediately preceding the state.
    block = peek(state_ptr - 0x10, 0x10)
    if len(block) < 0x10:
        print(f"  [{depth}] state=0x{state_ptr:08x}: short read")
        break
    tag = block[0x0C:0x10]  # s32 BE
    tag_int = struct.unpack(">I", tag)[0]
    tag_chars = tag.decode("latin-1", errors="replace")
    state = peek(state_ptr, 0x10)
    free_space = struct.unpack(">i", state[0x00:0x04])[0]
    prev       = struct.unpack(">I", state[0x0C:0x10])[0]
    print(f"  [{depth}] state=0x{state_ptr:08x}  tag={tag_chars!r} (0x{tag_int:08x})  "
          f"free=0x{free_space:08x}  prev=0x{prev:08x}")
    state_ptr = prev
    depth += 1
print(f"\nstack depth = {depth}")
