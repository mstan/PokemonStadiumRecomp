"""One-shot probe: query debug_server rings during the softlock."""
import socket, json, sys

HOST, PORT = "127.0.0.1", 4371

def call(cmd: str):
    s = socket.create_connection((HOST, PORT), timeout=4)
    s.sendall((cmd + "\n").encode())
    s.settimeout(3)
    buf = b""
    try:
        while True:
            chunk = s.recv(65536)
            if not chunk:
                break
            buf += chunk
            if buf.endswith(b"\n"):
                break
    except socket.timeout:
        pass
    s.close()
    raw = buf.decode(errors="replace").strip()
    try:
        return json.loads(raw)
    except Exception:
        return {"_raw": raw}

def banner(t):
    print()
    print("=" * 8, t, "=" * 8)

banner("ping")
print(call("ping"))

banner("status")
print(json.dumps(call("status"), indent=2))

banner("trace_recent n=120 (last function-trace ring entries)")
tr = call('{"cmd":"trace_recent","n":120}')
if "entries" in tr:
    es = tr["entries"]
    print(f"write_idx={tr.get('write_idx')} count={len(es)}")
    print("--- chronological ---")
    for i, e in enumerate(es):
        print(f"{i:3d}  {e}")
else:
    print(tr)

banner("libultra_recent n=80 (last libultra calls)")
lu = call('{"cmd":"libultra_recent","n":80}')
if "events" in lu:
    es = lu["events"]
    print(f"write_idx={lu.get('write_idx')} count={len(es)}")
    for e in es:
        print(
            f"  i={e['i']:>5} ms={e['ms']:>8} pc=0x{e['pc']:08x} "
            f"{e['name']:<28} "
            f"a0=0x{e['a0']:08x} a1=0x{e['a1']:08x} a2=0x{e['a2']:08x} a3=0x{e['a3']:08x}"
        )
else:
    print(lu)
