"""Search ROM (specifically menu_select_ui region) for the Vtx pattern we
just saw at RDRAM 0x8011D460. The first Vtx is:
  V0 xyz=(0,76,0) st=(0,0) rgba=FFFFFFFF
encoded as: 00 00 00 4C 00 00 00 00 00 00 00 00 FF FF FF FF
"""
import struct

ROM = 'F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64'
SEARCH_START = 0x4EB0C0
SEARCH_END   = 0x5046B0

# The exact 16-byte Vtx record for quad 0 V0
NEEDLE = struct.pack('>hhhhhhBBBB', 0, 76, 0, 0, 0, 0, 255, 255, 255, 255)

with open(ROM, 'rb') as f:
    f.seek(SEARCH_START)
    data = f.read(SEARCH_END - SEARCH_START)

hits = []
i = 0
while True:
    j = data.find(NEEDLE, i)
    if j < 0: break
    hits.append(j)
    i = j + 1

print(f'Searching menu_select_ui  [0x{SEARCH_START:08X}..0x{SEARCH_END:08X}]  for first-Vtx pattern')
print(f'Found {len(hits)} matches:')
for j in hits:
    rom_off = SEARCH_START + j
    # Dump next 4 Vtx (one quad) to confirm it's a real Vtx grid
    print(f'\n  ROM 0x{rom_off:08X}  (menu_select_ui +0x{j:05X})')
    for v in range(4):
        x, y, z, fl, s, t, r, g, b, a = struct.unpack('>hhhhhhBBBB', data[j+v*16:j+v*16+16])
        print(f'    V{v}: xyz=({x:5d},{y:5d},{z:5d}) st=({s:5d},{t:5d}) rgba=({r},{g},{b},{a})')
