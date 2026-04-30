"""Dump recent trace ring entries (need active runner)."""
import socket, json, sys

n = int(sys.argv[1]) if len(sys.argv) > 1 else 128

s = socket.create_connection(("127.0.0.1", 4371), timeout=3)
s.sendall((json.dumps({"cmd":"trace_recent","n":n}) + "\n").encode())
buf = b""
while not buf.endswith(b"\n"):
    c = s.recv(65536)
    if not c: break
    buf += c
s.close()
r = json.loads(buf.decode().strip())
ents = r.get("entries", [])
print(f"trace ring last {len(ents)}:")
# Aggregate consecutive duplicates
from collections import Counter
counts = Counter(ents)
print(f"unique: {len(counts)}")
print("top 20 most-frequent:")
for name, cnt in counts.most_common(20):
    print(f"  {cnt:6d}  {name}")
print()
print(f"last {min(40,len(ents))} chronologically:")
for e in ents[-40:]:
    print(f"  {e}")
