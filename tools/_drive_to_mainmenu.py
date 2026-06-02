"""Drive the runner from boot through Game Pak Check to the Main
Menu. Used to script reproducible state navigation without a human
in the loop.

Usage:
  python tools/_drive_to_mainmenu.py
"""
import socket, json, time, sys, os, subprocess, pathlib

ROOT = pathlib.Path('F:/Projects/n64recomp/PokemonStadiumRecomp')
EXE = ROOT / 'build' / 'PokemonStadiumRecomp.exe'
HOST = ('127.0.0.1', 4371)


def call(cmd, timeout=4):
    try:
        s = socket.create_connection(HOST, timeout=timeout)
    except OSError:
        return None
    try:
        s.sendall((json.dumps(cmd) + '\n').encode())
        buf = b''
        s.settimeout(timeout)
        while True:
            try:
                c = s.recv(65536)
            except socket.timeout:
                break
            if not c: break
            buf += c
            if buf.endswith(b'\n'): break
    finally:
        s.close()
    try:
        return json.loads(buf.decode().strip())
    except Exception:
        return None


def wait_for_boot(target=120, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        st = call({'cmd':'status'})
        if st and st.get('frame', 0) >= target:
            return st
        time.sleep(0.5)
    return None


def press(button, hold=0.15, post=0.4):
    call({'cmd':'set_button', 'name':button, 'down':True})
    time.sleep(hold)
    call({'cmd':'set_button', 'name':button, 'down':False})
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
    # 1. Launch if not running.
    st = call({'cmd':'status'})
    if st is None:
        print('launching runner...')
        env = os.environ.copy()
        # keep logs quiet
        subprocess.Popen(['cmd', '/c', 'start', '', str(EXE)],
                         cwd=str(ROOT), env=env)
        time.sleep(3)

    # 2. Wait for game thread to be running.
    st = wait_for_boot(target=120, timeout=90)
    if not st:
        print('boot timeout')
        return 1
    print(f'boot OK: frame={st["frame"]}')
    screenshot('00_boot')

    # 3. Take control.
    print('claiming input...')
    call({'cmd':'claim_input'})
    time.sleep(0.5)

    # 4. Mash through Game Pak Check / title / OK box.
    # Title screen needs Start; Game Pak Check OK needs A.
    # Button names are case-sensitive (see debug_server.cpp button_bit).
    for i, btn in enumerate(['Start', 'Start', 'A', 'A', 'A', 'A', 'A', 'A', 'A']):
        print(f'  press {btn}')
        press(btn, hold=0.2, post=1.0)
        screenshot(f'{i+1:02d}_after_{btn}')

    # 5. Release control.
    call({'cmd':'clear_input'})
    st = call({'cmd':'status'})
    print(f'final status: frame={st.get("frame")}')


if __name__ == '__main__':
    sys.exit(main() or 0)
