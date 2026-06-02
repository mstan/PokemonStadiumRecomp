"""Probe TCP debug server for Petit Cup softlock state.

Always-on ring buffers per global rule — query, don't arm.
"""
import json
import socket
import sys


def call(cmd: dict, timeout: float = 5.0) -> str:
    s = socket.socket()
    s.settimeout(timeout)
    s.connect(("127.0.0.1", 4371))
    payload = (json.dumps(cmd) + "\n").encode()
    s.sendall(payload)
    buf = b""
    while True:
        try:
            c = s.recv(65536)
        except socket.timeout:
            break
        if not c:
            break
        buf += c
        if buf.endswith(b"\n"):
            break
    s.close()
    return buf.decode(errors="replace").strip()


probes = [
    "dump_threads",
    "get_last_pc_trail",
    "libultra_recent",
    "mesg_recent",
    "post_mortem_dump",
]
which = sys.argv[1] if len(sys.argv) > 1 else "dump_threads"
print(f"=== {which} ===")
print(call({"cmd": which}))
