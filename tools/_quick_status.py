"""One-shot TCP status query for capture-and-document workflows."""
import json, socket
s = socket.create_connection(('127.0.0.1', 4371), timeout=3)
s.sendall(b'{"cmd":"status"}\n')
buf = b''
while True:
    c = s.recv(8192)
    if not c: break
    buf += c
    if buf.endswith(b'\n'): break
s.close()
print(json.dumps(json.loads(buf.decode()), indent=2))
