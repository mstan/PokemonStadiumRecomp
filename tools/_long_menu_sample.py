"""Watch send_dl + game state over 30s in MENU_SELECT."""
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

print("t   frame   send_dl   gs")
last_dl = None
for i in range(30):
    st = call({"cmd":"status"})
    gs = call({"cmd":"rdram_peek","addr":0x80075668,"n":4})
    dl = st.get("send_dl")
    delta = (dl - last_dl) if last_dl is not None else 0
    last_dl = dl
    print(f"{i:3}  {st.get('frame'):5}  {dl:5} (+{delta:3})  gs={gs.get('hex')}")
    time.sleep(1.0)
