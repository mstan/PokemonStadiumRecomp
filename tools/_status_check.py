import socket
try:
    s = socket.socket(); s.settimeout(2); s.connect(('127.0.0.1',4371))
    s.sendall(b'{"cmd":"status"}\n')
    print(s.recv(4096).decode())
except Exception as e:
    print('runner not reachable:', e)
