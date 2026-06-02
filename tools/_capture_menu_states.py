"""Drive back to Main Menu from current location (pressing B a few
times), then take a screenshot. Used to capture each menu state for
visual-corruption documentation.
"""
import socket, json, time, sys, subprocess, pathlib

ROOT = pathlib.Path('F:/Projects/n64recomp/PokemonStadiumRecomp')

def call(cmd, timeout=4):
    s = socket.create_connection(('127.0.0.1',4371), timeout=timeout)
    s.sendall((json.dumps(cmd)+'\n').encode())
    buf=b''
    s.settimeout(timeout)
    while True:
        try:c=s.recv(65536)
        except socket.timeout:break
        if not c:break
        buf+=c
        if buf.endswith(b'\n'):break
    s.close()
    return json.loads(buf.decode().strip()) if buf else None

def press(btn,hold=0.2,post=1.0):
    call({'cmd':'set_button','name':btn,'down':True})
    time.sleep(hold)
    call({'cmd':'set_button','name':btn,'down':False})
    time.sleep(post)

def shot(label):
    out=ROOT/'build'/f'state_{label}.png'
    subprocess.run(['powershell','-ExecutionPolicy','Bypass','-File',
        str(ROOT/'tools'/'_screenshot.ps1'), str(out)],
        check=False, capture_output=True)
    return out

call({'cmd':'claim_input'})
time.sleep(0.3)

# Press B many times to get back to Main Menu from wherever
for i in range(10):
    press('B', post=0.5)
    shot(f'back_{i:02d}')

call({'cmd':'clear_input'})
print('done')
