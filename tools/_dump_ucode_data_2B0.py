"""Dump ucode_data[0x2B0..0x400] from ROM to see what the hook
overwrites with the audio command list."""
import hashlib

rom = open('F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64', 'rb').read()
md5 = hashlib.md5(rom).hexdigest()
assert md5 == 'ed1378bc12115f71209a77844965ba50', f'bad rom {md5}'

# ucode_data is at ROM 0x7F670 (per toml).
ucode_data_rom = 0x7F670

# DMEM[0x000..0x020] = dispatch table (verified)
# DMEM[0x020..0x???] = ???
# DMEM[0x2B0..0x3F0] = where the hook DMAs audio commands TO
# DMEM[0xFC0..0x1000] = OSTask scratch

# Show:
#  - first 0x20 (dispatch table)
#  - 0x20..0x60 (immediate post-table area)
#  - 0x2B0..0x400 (the area the hook overwrites)
def dump(label, off, size):
    print(f'\n--- {label} ROM=0x{ucode_data_rom + off:X} DMEM[0x{off:X}..0x{off+size:X}] ---')
    chunk = rom[ucode_data_rom + off : ucode_data_rom + off + size]
    nonzero = sum(1 for b in chunk if b != 0)
    print(f'  nonzero bytes: {nonzero}/{size}')
    for line in range(0, size, 16):
        hex_part = ' '.join(f'{b:02X}' for b in chunk[line:line+16])
        print(f'  +0x{off+line:04X}: {hex_part}')

dump('dispatch table', 0x000, 0x20)
dump('post-table (0x20..0x60)', 0x020, 0x40)
dump('hook-overwrite-region head (0x2B0..0x300)', 0x2B0, 0x50)
dump('hook-overwrite-region tail (0x380..0x3F0)', 0x380, 0x70)
