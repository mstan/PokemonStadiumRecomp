"""Find every FRAGMENT magic in the ROM, cross-reference with pret's rom.yaml."""
import struct
import re
import sys

ROM = 'F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64'
YAML = 'F:/Projects/n64recomp/PokemonStadiumRecomp/disasm/yamls/us/rom.yaml'

with open(ROM, 'rb') as f:
    rom = f.read()

# Find all aligned occurrences of "FRAGMENT" magic
matches = []
i = 0
while True:
    p = rom.find(b'FRAGMENT', i)
    if p < 0:
        break
    if (p & 7) == 0:
        # FRAGMENT magic is at fragment+0x08, so fragment header starts at p-8
        frag_start = p - 8
        if frag_start >= 0 and frag_start + 0x20 <= len(rom):
            inst0 = struct.unpack('>I', rom[frag_start:frag_start+4])[0]
            j_op = inst0 >> 26
            if j_op == 2:  # J instruction
                hdr_size, reloc_off, sz_a, sz_b = struct.unpack('>IIII', rom[frag_start+0x10:frag_start+0x20])
                # Decode link-vram from J target field + apparent fragment id
                j_target = (inst0 & 0x3FFFFFF) << 2
                # Stadium reloc applies (target & 0x0FFFFFFC) + 0x80000000 to get full vaddr
                full_target = (j_target & 0x0FFFFFFC) | 0x80000000
                bucket = (full_target & 0x0FF00000) >> 20
                frag_id = bucket - 0x10
                link_vram = (bucket << 20) | 0x80000000
                matches.append({
                    'rom': frag_start,
                    'inst0_J_target': j_target,
                    'full_J_target': full_target,
                    'frag_id': frag_id,
                    'link_vram': link_vram,
                    'header_size': hdr_size,
                    'reloc_offset': reloc_off,
                    'size_a (off 0x18)': sz_a,
                    'size_b (off 0x1C)': sz_b,
                })
    i = p + 1

# Load yaml fragment ROM offsets so we can match by start
frag_yaml = {}
with open(YAML) as fh:
    yaml_text = fh.read()
# Pattern: textbin fragments/N/fragmentN_header at ROM offset
for m in re.finditer(r'\[(0x[0-9A-Fa-f]+),\s*textbin,\s*fragments/(\d+)/fragment\d+_header\]', yaml_text):
    rom_off = int(m.group(1), 16)
    frag_n = int(m.group(2))
    frag_yaml[rom_off] = frag_n
# Also fragment1's hasm header
for m in re.finditer(r'\[(0x[0-9A-Fa-f]+),\s*hasm,\s*fragments/(\d+)/fragment\d+_header\]', yaml_text):
    rom_off = int(m.group(1), 16)
    frag_n = int(m.group(2))
    frag_yaml[rom_off] = frag_n

print(f'Total FRAGMENT magic occurrences in ROM: {len(matches)}')
print(f'Total fragments in pret yaml: {len(frag_yaml)}')
print()
print(f'{"ROM":>10}  {"id":>4} {"link_vram":>10} {"hdrsz":>6} {"reloff":>8} {"szA":>8} {"szB":>8}  pret#')
for m in matches:
    pret_n = frag_yaml.get(m['rom'], '???')
    flag = '' if pret_n != '???' else '   <-- MISSING from pret yaml'
    print(f'  0x{m["rom"]:08X}  0x{m["frag_id"]:02X}  0x{m["link_vram"]:08X}  0x{m["header_size"]:04X}  0x{m["reloc_offset"]:06X}  0x{m["size_a (off 0x18)"]:06X}  0x{m["size_b (off 0x1C)"]:06X}  {pret_n}{flag}')
