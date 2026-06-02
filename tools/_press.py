"""Press one button on the runner. Usage: python tools/_press.py A [hold_ms]"""
import socket, json, sys, time
HOST = ('127.0.0.1', 4371)


def call(cmd, t=4):
    s = socket.create_connection(HOST, timeout=t)
    s.sendall((json.dumps(cmd) + '\n').encode())
    buf = b''
    s.settimeout(t)
    while True:
        try:
            c = s.recv(65536)
        except socket.timeout:
            break
        if not c:
            break
        buf += c
        if buf.endswith(b'\n'):
            break
    s.close()
    try:
        return json.loads(buf.decode().strip())
    except Exception:
        return None


def main():
    if len(sys.argv) < 2:
        print('usage: python tools/_press.py <Button> [hold_ms]')
        return 1
    btn = sys.argv[1]
    hold = float(sys.argv[2]) / 1000.0 if len(sys.argv) > 2 else 0.25

    call({'cmd': 'claim_input'})
    time.sleep(0.05)
    print('down', btn, call({'cmd': 'set_button', 'name': btn, 'down': True}))
    time.sleep(hold)
    print('up', btn, call({'cmd': 'set_button', 'name': btn, 'down': False}))
    time.sleep(0.5)
    call({'cmd': 'clear_input'})
    st = call({'cmd': 'status'})
    print('frame', st.get('frame'))


if __name__ == '__main__':
    main()
