import socket, json, sys
path = sys.argv[1] if len(sys.argv) > 1 else "F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_dl_now.bin"
s = socket.socket(); s.settimeout(5); s.connect(("127.0.0.1",4371))
s.sendall((json.dumps({"cmd":"dump_current_dl","path":path})+"\n").encode())
print(s.recv(4096).decode())
