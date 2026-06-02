import struct
with open('baserom.z64','rb') as f:
    f.seek(0x002C49D0 + 0x7008)
    data = f.read(16*16)
for i in range(16):
    x, y, z, fl, s, t, r, g, b, a = struct.unpack('>hhhhhhBBBB', data[i*16:i*16+16])
    print(f'  +0x{0x7008+i*16:04X}: xyz=({x:5d},{y:5d},{z:5d}) flag={fl:#06x} st=({s:5d},{t:5d}) rgba=({r},{g},{b},{a})')
