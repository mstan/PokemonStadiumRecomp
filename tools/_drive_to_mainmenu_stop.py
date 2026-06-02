"""Conservative drive: Start, Start, A (Game Pak Check OK) then stop on Main Menu.

Usage: python tools/_drive_to_mainmenu_stop.py
"""
import socket, json, time, subprocess, pathlib

ROOT = pathlib.Path('F:/Projects/n64recomp/PokemonStadiumRecomp')
HOST = ('127.0.0.1', 4371)


def call(cmd, t=4):
    try:
        s = socket.create_connection(HOST, timeout=t)
    except OSError:
        return None
    try:
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
    finally:
        s.close()
    try:
        return json.loads(buf.decode().strip())
    except Exception:
        return None


def press(btn, hold=0.2, post=1.5):
    call({'cmd': 'set_button', 'name': btn, 'down': True})
    time.sleep(hold)
    call({'cmd': 'set_button', 'name': btn, 'down': False})
    time.sleep(post)


def screenshot(label):
    out = ROOT / 'build' / f'drive_{label}.png'
    subprocess.run([
        'powershell', '-ExecutionPolicy', 'Bypass', '-File',
        str(ROOT / 'tools' / '_screenshot.ps1'),
        str(out),
    ], check=False, capture_output=True)
    return out


def main():
    for _ in range(60):
        st = call({'cmd': 'status'})
        if st and st.get('frame', 0) >= 120:
            break
        time.sleep(0.5)
    print('boot at frame', st.get('frame'))

    call({'cmd': 'claim_input'})
    time.sleep(0.5)

    press('Start', post=2.0)
    press('Start', post=2.0)
    press('A',     post=3.0)  # Game Pak Check OK -> Main Menu

    call({'cmd': 'clear_input'})
    st = call({'cmd': 'status'})
    print('final frame', st.get('frame'))
    screenshot('mainmenu_stop')


if __name__ == '__main__':
    main()
