"""Search trace_recent for sched dispatch functions (case 0x64/0x65/...)."""
import socket, json
from collections import Counter

s = socket.create_connection(("127.0.0.1", 4371), timeout=4)
s.sendall(b'{"cmd":"trace_recent","n":512}\n')
s.settimeout(3)
buf = b""
import time
end = time.time() + 3
while time.time() < end:
    try:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b"\n"): break
    except Exception:
        break
s.close()
r = json.loads(buf.decode(errors="replace").strip())
es = r.get("entries", [])
c = Counter(es)
print("write_idx:", r.get("write_idx"))
print()
print("Sched fns of interest in last 512 entries:")
for name in [
    "func_80005084",  # case 0x64 SP DONE
    "func_80005148",  # case 0x65 DP DONE
    "func_80004B0C",  # SP DONE inner
    "func_80004C68",  # DP DONE inner
    "func_80004E94",  # case 0x67
    "func_80004F08",  # case 0x68 PreNMI
    "func_80004F70",  # case 0x66 VI
    "func_80004E60",  # task launcher
    "func_800049D4",  # actually-launches sp task
    "func_80005194",  # sched main loop
    "func_8000183C",  # gfx_sched main (thread 5 entry)
    "func_8000152C",
    "func_800015A8",
]:
    print(f"  {name:25}: {c.get(name, 0)}")
