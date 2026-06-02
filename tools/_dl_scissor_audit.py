"""Walk a DL and dump every G_SETSCISSOR and G_MOVEMEM(viewport).

G_SETSCISSOR (0xED): w0 = [op:8, ulx:12, uly:12]; w1 = [mode:8, lrx:12, lry:12].
  Coords are in S10.2 fixed-point (4 fractional bits per FBE/pret).
G_MOVEMEM (0xDB): w0 = [op:8, size:8, offset:8, idx:8]; w1 = vaddr.
  idx=8 (G_MV_VIEWPORT): the target is a Vp struct (16B): vscale[4], vtrans[4].
  Vp values are also S15.16 in screen-pixel * 4 (i.e. 1 unit = 0.25 pixel).
"""
import struct, sys, socket, json

PATH = sys.argv[1] if len(sys.argv) > 1 else 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/dl_mainmenu_clip.bin'

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


def rdram_peek(addr, n):
    r = call({'cmd': 'rdram_peek', 'addr': addr, 'n': n})
    return bytes.fromhex(r['hex']) if r else None


with open(PATH, 'rb') as f:
    data = f.read()

print(f'DL: {PATH}  ({len(data)} bytes)\n')
print('=== G_SETSCISSOR events ===')
i = 0
seen_scissor = 0
while i + 8 <= len(data):
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xED:
        ulx = (w0 >> 12) & 0xFFF
        uly = (w0 >>  0) & 0xFFF
        mode = (w1 >> 24) & 0xFF
        lrx = (w1 >> 12) & 0xFFF
        lry = (w1 >>  0) & 0xFFF
        # Each is 10.2 fixed-point
        print(f'  +{i:#06x}  ulx={ulx/4:6.2f} uly={uly/4:6.2f}'
              f'  lrx={lrx/4:6.2f} lry={lry/4:6.2f}  mode=0x{mode:02X}')
        seen_scissor += 1
    elif op == 0xDB:
        size = (w0 >> 16) & 0xFF
        offs = (w0 >>  8) & 0xFF
        idx  = (w0 >>  0) & 0xFF
        if idx == 8:  # G_MV_VIEWPORT
            print(f'\n  +{i:#06x}  G_MOVEMEM(viewport)  vaddr=0x{w1:08X}  size={size} offset={offs}')
            # Peek the Vp struct at w1
            vp = rdram_peek(w1, 16)
            if vp:
                vsx, vsy, vsz, vsg = struct.unpack('>hhhh', vp[0:8])
                vtx, vty, vtz, vtg = struct.unpack('>hhhh', vp[8:16])
                # screen_pixel = vt + vs * ndc, with vs and vt in pixel*4 units
                print(f'    vscale=(x={vsx/4:.2f}, y={vsy/4:.2f}, z={vsz/4:.2f})')
                print(f'    vtrans=(x={vtx/4:.2f}, y={vty/4:.2f}, z={vtz/4:.2f})')
                # ndc range is [-1, +1], so on-screen y range = vt.y +/- vs.y
                lo_y = (vty - vsy) / 4.0
                hi_y = (vty + vsy) / 4.0
                lo_x = (vtx - vsx) / 4.0
                hi_x = (vtx + vsx) / 4.0
                print(f'    screen X: [{lo_x:.2f}, {hi_x:.2f}]   Y: [{lo_y:.2f}, {hi_y:.2f}]')
    i += 8

print(f'\nTotal scissors: {seen_scissor}')
