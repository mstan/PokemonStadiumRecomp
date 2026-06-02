import socket, json, sys
try:
    s = socket.create_connection(('127.0.0.1', 4371), timeout=2)
    s.sendall(b'{"cmd":"status"}\n')
    print(s.recv(4096).decode())
    s.close()
except Exception as e:
    print(f'ERR: {e}')
