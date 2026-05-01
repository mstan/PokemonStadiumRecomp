"""Press Start now and watch send_dl for 5s."""
import json, socket, time

def call(cmd, t=2.0):
    s=socket.create_connection(("127.0.0.1",4371),timeout=t)
    s.sendall((json.dumps(cmd)+"\n").encode())
    s.settimeout(t); buf=b""
    while True:
        try: c=s.recv(65536)
        except: break
        if not c: break
        buf+=c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode().strip())

print("before:", call({"cmd":"status"}))
call({"cmd":"set_button","name":"Start","down":True}); time.sleep(0.15)
call({"cmd":"set_button","name":"Start","down":False})
for i in range(6):
    time.sleep(1.0)
    s = call({"cmd":"status"})
    print(f"+{i+1}s frame={s.get('frame')} send_dl={s.get('send_dl')} update_screen={s.get('update_screen')}")
