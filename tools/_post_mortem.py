"""Trigger psr_post_mortem_dump via TCP. Writes build/last_run_report.json."""
import socket, json

s = socket.create_connection(('127.0.0.1', 4371), timeout=15)
s.sendall((json.dumps({'cmd': 'post_mortem_dump'}) + '\n').encode())
buf = b''
s.settimeout(15)
while True:
    try:
        c = s.recv(65536)
    except socket.timeout:
        break
    if not c:
        break
    buf += c
    if buf.endswith(b'\n'):
        break
s.close()
print(buf.decode().strip())
