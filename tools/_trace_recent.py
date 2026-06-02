"""Pull last N function-trace entries (pkmnstadium_trace_entry hits)."""
import socket, json, sys, collections

HOST = ('127.0.0.1', 4371)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 256


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


d = call({'cmd': 'trace_recent', 'n': N})
print('write_idx=', d.get('write_idx'))
entries = d.get('entries', [])
# Sequential view
for i, e in enumerate(entries[-N:]):
    print(f'  {i:>4} {e}')
# Counter view of recent
print('\nTop functions (last %d entries):' % len(entries))
for name, count in collections.Counter(entries).most_common(20):
    print(f'  {count:>5} {name}')
