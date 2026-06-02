"""Pull recent OSMesgQueue events from the always-on ring."""
import socket, json, sys, collections

HOST = ('127.0.0.1', 4371)
N = int(sys.argv[1]) if len(sys.argv) > 1 else 200


def call(cmd, t=10):
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


d = call({'cmd': 'mesg_recent', 'n': N})
events = d.get('events', [])
print(f'write_idx={d.get("write_idx")}  events={len(events)}')
# Filter to game-thread-only events for the stall signature
gt_events = [e for e in events if e.get('gt') == 1]
print(f'game-thread events: {len(gt_events)}')
print()
print('Last 30 game-thread events:')
for e in gt_events[-30:]:
    print(f"  seq={e['seq']:>8} ms={e['ms']:>10} {e['op']:<16} mq=0x{e['mq']:08X} msg=0x{e['msg']:08X} vb={e['vb']} va={e['va']} block={e['block']}")

# Last events overall
print('\nLast 20 events (all threads):')
for e in events[-20:]:
    gt_marker = 'GT' if e.get('gt') == 1 else '  '
    print(f"  {gt_marker} seq={e['seq']:>8} ms={e['ms']:>10} {e['op']:<16} mq=0x{e['mq']:08X} msg=0x{e['msg']:08X} vb={e['vb']} va={e['va']}")

# Op histogram
print('\nOp histogram:')
for op, cnt in collections.Counter(e['op'] for e in events).most_common():
    print(f'  {cnt:>5} {op}')
