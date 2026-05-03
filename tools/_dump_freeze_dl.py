"""Dump the bytes of the hung gfx DL (task #1157) to disk for offline
GBI decoding. data_ptr/data_size come from the sp_task_recent ring's
last M_GFXTASK entry."""
import json, socket, sys

DATA_PTR = 0x80209500
DATA_SIZE = 16800
OUT_PATH = "build/freeze_dl_seq4090.bin"

s = socket.create_connection(("127.0.0.1", 4371), timeout=5)
out = bytearray()
chunk = 4096
off = 0
while off < DATA_SIZE:
    n = min(chunk, DATA_SIZE - off)
    s.sendall((json.dumps({"cmd": "rdram_peek", "addr": DATA_PTR + off, "n": n}) + "\n").encode())
    s.settimeout(5)
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
    r = json.loads(buf.decode().strip())
    out.extend(bytes.fromhex(r["hex"]))
    off += n
s.close()
with open(OUT_PATH, "wb") as f:
    f.write(out)
print(f"wrote {len(out)} bytes to {OUT_PATH}")
print(f"first 64 bytes hex: {out[:64].hex()}")
