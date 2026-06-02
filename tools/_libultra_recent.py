"""Pull recent libultra-call events from the always-on trace ring."""
import socket, json, sys

HOST = ('127.0.0.1', 4371)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 60


def call(cmd, t=4):
    s = socket.create_connection(HOST, timeout=t)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    s.settimeout(t)
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
    return json.loads(buf.decode().strip())


d = call({'cmd': 'libultra_recent', 'n': N})
print('write_idx=', d.get('write_idx'))
for ev in d.get('events', []):
    name = ev.get('name', '?')
    pc = ev.get('pc', 0)
    a0 = ev.get('a0', 0)
    a1 = ev.get('a1', 0)
    a2 = ev.get('a2', 0)
    a3 = ev.get('a3', 0)
    ms = ev.get('ms', 0)
    print(f"  i={ev['i']:>6} {name:28s} pc=0x{pc:08X} a0=0x{a0:08X} a1=0x{a1:08X} a2=0x{a2:08X} a3=0x{a3:08X} ms={ms}")
