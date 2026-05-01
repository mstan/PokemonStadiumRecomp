"""Sample status every 2s for ~30s, print frame/send_dl/vi to see if rendering progresses."""
import json, socket, sys, time

def call(cmd, timeout=2.0):
    try:
        s = socket.create_connection(("127.0.0.1", 4371), timeout=timeout)
    except OSError:
        return {"_err": "connect"}
    try:
        s.sendall((json.dumps(cmd) + "\n").encode())
        s.settimeout(timeout)
        buf = b""
        while True:
            try:
                c = s.recv(65536)
            except socket.timeout:
                break
            if not c: break
            buf += c
            if buf.endswith(b"\n"): break
    finally:
        s.close()
    try: return json.loads(buf.decode(errors="replace").strip())
    except: return {"_err": "parse"}

print("t  frame   vi      send_dl  send_dl_gfx  pc_trail (last 4)")
t0 = time.time()
for i in range(20):
    st = call({"cmd": "status"})
    pc = call({"cmd": "get_last_pc_trail"})
    pct = pc.get("pc_trail", []) if isinstance(pc, dict) else []
    print(f"{time.time()-t0:5.1f}  {st.get('frame'):5}  {st.get('vi'):5}  {st.get('send_dl'):5}    {st.get('send_dl_gfx'):5}        {pct[-4:] if pct else '-'}  watchdog={pc.get('watchdog_count') if isinstance(pc,dict) else '?'}")
    time.sleep(2.0)
