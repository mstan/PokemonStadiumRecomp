import socket, json, sys
count = int(sys.argv[1]) if len(sys.argv) > 1 else 8
start = int(sys.argv[2]) if len(sys.argv) > 2 else -1
s = socket.socket()
s.connect(('127.0.0.1', 4371))
msg = {"cmd": "vtx_ring", "count": count}
if start >= 0:
    msg["start_seq"] = start
s.sendall((json.dumps(msg) + "\n").encode())
buf = b""
while True:
    chunk = s.recv(65536)
    if not chunk:
        break
    buf += chunk
    if b"\n" in chunk or b"}]}" in buf:
        break
data = json.loads(buf.decode())
print("write_idx:", data.get("write_idx"), "capacity:", data.get("capacity"),
      "start_seq:", data.get("start_seq"))
for e in data.get("entries", []):
    print(f"seq={e['seq']:6d}  v0={e['v0']}  v1={e['v1']}  v2={e['v2']}")
