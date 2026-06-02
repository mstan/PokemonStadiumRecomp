import socket, json
s = socket.create_connection(('127.0.0.1',4371), timeout=2)
s.sendall(b'{"cmd":"rdram_peek","addr":2194407432,"n":16}\n')  # 0x82D07008
print(s.recv(4096).decode())
s.close()
