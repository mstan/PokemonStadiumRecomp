#!/usr/bin/env python3
# Generic debug-server command sender. Usage:
#   python tools/_cmd.py <cmd> [json_extra_args]
# Examples:
#   python tools/_cmd.py rt64_segments
#   python tools/_cmd.py dump_current_dl "{\"path\":\"build/dl.bin\"}"
import socket, json, sys

HOST = ('127.0.0.1', 4371)


def call(obj):
    s = socket.create_connection(HOST, timeout=10)
    s.sendall((json.dumps(obj) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return buf.decode().strip()


def main():
    obj = {'cmd': sys.argv[1]}
    if len(sys.argv) > 2:
        obj.update(json.loads(sys.argv[2]))
    print(call(obj))


if __name__ == '__main__':
    main()
