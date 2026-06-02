"""Verify aspMain dispatch handler table at ucode_data ROM 0x7F670."""
import hashlib
import sys

rom = open('F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64', 'rb').read()
md5 = hashlib.md5(rom).hexdigest()
assert md5 == 'ed1378bc12115f71209a77844965ba50', f'bad rom {md5}'

# 16 big-endian halfwords at start of ucode_data
b = rom[0x7F670:0x7F690]
actual = [(b[i*2] << 8) | b[i*2+1] for i in range(16)]
print('ROM[0x7F670..0x7F690]:', ' '.join(f'{v:04X}' for v in actual))

toml_claimed = [
    0x10EC, 0x139C, 0x119C, 0x1A64, 0x11C8, 0x17EC, 0x1208, 0x0000,
    0x0000, 0x127C, 0x1348, 0x1248, 0x1C84, 0x12D4, 0x02B0, 0x1384,
]
print('toml claimed:        ', ' '.join(f'{v:04X}' for v in toml_claimed))

if actual == toml_claimed:
    print('MATCH')
else:
    print('DIFF:')
    for i, (a, t) in enumerate(zip(actual, toml_claimed)):
        if a != t:
            print(f'  slot[{i}]: ROM={a:04X}  toml={t:04X}')
