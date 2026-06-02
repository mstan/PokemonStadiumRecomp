import struct, sys
PATH = sys.argv[1]
TARGET = int(sys.argv[2], 16)
BACK = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x100

data = open(PATH,'rb').read()

i = 0
last_combine_off = None
while i < TARGET:
    w0 = struct.unpack('>I', data[i:i+4])[0]
    op = (w0 >> 24) & 0xFF
    if op == 0xFC:
        last_combine_off = i
    i += 8
print(f'Last SETCOMBINE before {TARGET:#06x} is at +{last_combine_off:#06x}' if last_combine_off else 'No prior SETCOMBINE')

start = max(0, TARGET - BACK)
print(f'Cmds in [{start:#06x}, {TARGET+0x18:#06x}]:')
i = start
while i < TARGET + 0x18 and i + 8 <= len(data):
    w0 = struct.unpack('>I', data[i:i+4])[0]
    w1 = struct.unpack('>I', data[i+4:i+8])[0]
    op = (w0 >> 24) & 0xFF
    name = {
        0xFC:'COMB',0xFB:'ENV',0xFA:'PRIM',0xFD:'TIMG',
        0xF5:'TILE',0xF2:'TSIZ',0xF3:'LOAD',0xE4:'TRECT',
        0xF1:'HALF2',0xE1:'HALF1',0xE2:'OML',0xE3:'OMH',
        0xE6:'LSYNC',0xE7:'PSYNC',0xE8:'TSYNC',0xE9:'FSYNC',
        0xD7:'TEXTURE',0xDF:'ENDDL',0xDE:'DL',0xDB:'MOVEW'
    }.get(op, f'{op:#04x}')
    extra = ''
    if op == 0xFC:
        extra = '  [SETCOMBINE]'
    elif op == 0xFB:
        extra = f'  rgba=({(w1>>24)&0xFF},{(w1>>16)&0xFF},{(w1>>8)&0xFF},{w1&0xFF})'
    elif op == 0xFA:
        extra = f'  rgba=({(w1>>24)&0xFF},{(w1>>16)&0xFF},{(w1>>8)&0xFF},{w1&0xFF})'
    print(f'  +{i:#06x}  {name:7s}  w0={w0:08x}  w1={w1:08x}{extra}')
    i += 8
