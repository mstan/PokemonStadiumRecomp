"""Trigger an in-process post_mortem_dump over TCP."""
import json, socket, sys
s = socket.create_connection(("127.0.0.1", 4371), timeout=15)
s.sendall(b'{"cmd":"post_mortem_dump"}\n')
buf = b""
while True:
    c = s.recv(65536)
    if not c: break
    buf += c
    if buf.endswith(b"\n"): break
s.close()
print(buf.decode().strip())
