"""Deep diagnostic snapshot during attract stall."""
import json, socket

HOST, PORT = '127.0.0.1', 4371

def send(cmd):
    s = socket.create_connection((HOST, PORT), timeout=10)
    s.sendall((json.dumps(cmd) + "\n").encode())
    buf = b''
    while True:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b'\n'): break
    s.close()
    return json.loads(buf.decode())

print("=== STATUS ===")
st = send({"cmd": "status"})
for k, v in st.items():
    print(f"  {k}: {v}")

print("\n=== INTERESTING FNS (non-evicting counters) ===")
r = send({"cmd": "interesting_fns"})
for fn in r.get("fns", []):
    print(f"  {fn['name']:>40}: {fn['count']}")

print("\n=== LIBULTRA_RECENT 256 ===")
r = send({"cmd": "libultra_recent", "n": 256})
for ev in r.get("events", []):
    print(f"  i={ev['i']:>5} ms={ev['ms']:>10} {ev['name']:>22} pc=0x{ev['pc']:08X} a0=0x{ev['a0']:08X} a1=0x{ev['a1']:08X}")

print("\n=== DUMP THREADS -> last_error.log ===")
r = send({"cmd": "dump_threads"})
print(f"  {r}")
