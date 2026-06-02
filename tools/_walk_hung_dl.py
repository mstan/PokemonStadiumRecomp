"""Walk the captured hung DL and find what's at the target the last G_DL jumped to."""
import struct, sys

DL_START_VADDR = 0x80191FC0
TARGET_VADDR = 0x80192F08  # from the last cf event before runaway

with open('F:/Projects/n64recomp/PokemonStadiumRecomp/build/softlock_hung_dl.bin', 'rb') as f:
    data = f.read()

print(f'DL captured: {len(data)} bytes starting at vaddr 0x{DL_START_VADDR:08X}')
print(f'Target of last G_DL: vaddr 0x{TARGET_VADDR:08X}  (offset 0x{TARGET_VADDR - DL_START_VADDR:X} in capture)')

off = TARGET_VADDR - DL_START_VADDR
if off < 0 or off >= len(data):
    print(f'Target is OUT OF CAPTURED DL ({off:#x} not in [0, {len(data):#x}))')
    sys.exit(1)

# Walk forward from target. Look for ANY G_ENDDL (0xDF) within 128 commands.
print(f'\nWalking from offset 0x{off:X}:')
i = off
nopcount = 0
opcount = 0
got_enddl = False
for cmd_idx in range(128):
    if i + 8 > len(data):
        print(f'  +{i:#06x} -- past end of captured DL --')
        break
    w0, w1 = struct.unpack('>II', data[i:i+8])
    op = (w0 >> 24) & 0xFF
    name = {
        0x00: 'G_NOOP/G_SPNOOP',
        0xDE: 'G_DL',
        0xDF: 'G_ENDDL',
        0xD7: 'G_TEXTURE',
        0xD9: 'G_SETOTHERMODE_H',
        0xDA: 'G_MTX',
        0xDB: 'G_MOVEMEM',
        0xDC: 'G_MOVEWORD',
        0xE4: 'G_TEXRECT',
        0xE7: 'G_RDPPIPESYNC',
        0xF1: 'G_TRI2',
        0xF5: 'G_SETTILE',
        0xFC: 'G_SETCOMBINE',
        0xFD: 'G_SETTIMG',
    }.get(op, f'op_0x{op:02X}')
    flag = ''
    if op == 0xDE:
        push = (w0 >> 16) & 0xFF
        flag = f' push={push} target=0x{w1:08X}'
    print(f'  +{i:#06x} cmd[{cmd_idx:>2}] w0=0x{w0:08X} w1=0x{w1:08X}  op=0x{op:02X} {name}{flag}')
    opcount += 1
    if op == 0x00:
        nopcount += 1
    if op == 0xDF:
        got_enddl = True
        print(f'\n  → G_ENDDL found at +{i:#x}, after {cmd_idx} commands ({nopcount} were G_NOOP)')
        break
    if op == 0xDE:
        # G_DL — show the call target's first 4 commands
        push = (w0 >> 16) & 0xFF
        tgt_off = w1 - DL_START_VADDR
        if 0 <= tgt_off < len(data):
            print(f'    [target at vaddr 0x{w1:08X} = +0x{tgt_off:X} in capture, push={push}]')
        else:
            print(f'    [target at vaddr 0x{w1:08X} OUT OF CAPTURED DL]')
        # Continue walking (BRANCH doesn't push, CALL does — but we're tracing what RT64 does,
        # which is to follow the call OR continue sequentially after a branch). For NOW just
        # walk sequentially past G_DL to see what's after.
    i += 8

if not got_enddl:
    print(f'\n  *** NO G_ENDDL FOUND within {opcount} commands starting at the target ***')
    print(f'  *** That matches the symptom — RT64 walks sequentially forever ***')
    print(f'  NOOP count: {nopcount}/{opcount}')
