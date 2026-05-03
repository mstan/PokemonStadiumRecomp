"""Query the always-on osSpTaskStartGo ring; identify the LAST M_GFXTASK
before send_dl froze."""
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

r = call({"cmd": "sp_task_recent", "n": 4096})
evs = r["events"]
print(f"write_idx={r['write_idx']}  events_returned={len(evs)}")
print()
print("LAST 25 events:")
hdr = f"{'seq':>5}  {'ms':>7}  {'frame':>5}  {'send_dl':>7}  {'type':<11}  {'mips_ra':>10}  {'ucode':>10}  {'data_sz':>7}"
print(hdr)
print("-" * len(hdr))
for e in evs[-25:]:
    print(f"{e['seq']:>5}  {e['ms']:>7}  {e['frame']:>5}  {e['send_dl']:>7}  "
          f"{e['type']:<11}  {e['mips_ra']:#010x}  {e['ucode']:#010x}  {e['data_size']:>7}")

print()
gfx = [e for e in evs if e["type"] == "M_GFXTASK"]
aud = [e for e in evs if e["type"] == "M_AUDTASK"]
print(f"gfx count in window: {len(gfx)}")
print(f"audio count in window: {len(aud)}")

if gfx:
    print()
    print("LAST M_GFXTASK:")
    print(json.dumps(gfx[-1], indent=2))
    if len(gfx) >= 2:
        print()
        print("PRIOR M_GFXTASK:")
        print(json.dumps(gfx[-2], indent=2))
else:
    print("NO M_GFXTASK in window")

if aud:
    print()
    print("LAST M_AUDTASK:")
    print(json.dumps(aud[-1], indent=2))
