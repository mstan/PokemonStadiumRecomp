import socket, sys
v = float(sys.argv[1]) if len(sys.argv) > 1 else 1.0
s = socket.socket()
s.connect(('127.0.0.1', 4371))
s.sendall(('{"cmd":"set_volume","value":%f}\n' % v).encode())
print(s.recv(4096).decode())
