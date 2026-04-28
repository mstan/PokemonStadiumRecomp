"""Dump the FRAGMENT at ROM 0x1206400 (the unrecompiled one)."""
import struct
import sys

ROM_OFFSET = 0x1206400

with open('F:/Projects/PokemonStadiumRecomp/baserom.z64', 'rb') as f:
    f.seek(ROM_OFFSET)
    data = f.read(0x20000)

inst0, inst1 = struct.unpack('>II', data[:8])
magic = data[8:16]
hdr_size, reloc_off, size_rom, size_ram = struct.unpack('>IIII', data[16:32])

print(f'ROM offset = 0x{ROM_OFFSET:X}')
print(f'inst[0] = 0x{inst0:08X}   inst[1] = 0x{inst1:08X}')
print(f'magic   = {magic!r}')
print(f'headerSize  = 0x{hdr_size:X}')
print(f'relocOffset = 0x{reloc_off:X}')
print(f'sizeInRom   = 0x{size_rom:X}')
print(f'sizeInRam   = 0x{size_ram:X}')

j_op = inst0 >> 26
j_target = (inst0 & 0x3FFFFFF) << 2
print(f'J opcode = 0x{j_op:02X} (J expected = 2), J target field = 0x{j_target:08X}')

# Reloc table is at offset reloc_off within the fragment.
reloc_region = data[reloc_off:size_ram if size_ram > reloc_off else reloc_off + 0x1000]
n_relocs = struct.unpack('>I', reloc_region[:4])[0]
print()
print(f'nRelocations = {n_relocs}')

hist = {}
for i in range(n_relocs):
    if 4 + i*4 + 4 > len(reloc_region):
        print(f'  WARN: reloc region truncated at i={i}')
        break
    r = struct.unpack('>I', reloc_region[4 + i*4:8 + i*4])[0]
    rtype = (r >> 24) & 0x7F
    hist[rtype] = hist.get(rtype, 0) + 1

print(f'reloc type histogram: {hist}')
print()
print('first 8 relocs:')
type_name = {2:'R_MIPS_32', 4:'R_MIPS_26', 5:'R_MIPS_HI16', 6:'R_MIPS_LO16'}
for i in range(min(8, n_relocs)):
    r = struct.unpack('>I', reloc_region[4 + i*4:8 + i*4])[0]
    rtype = (r >> 24) & 0x7F
    off = r & 0xFFFFFF
    name = type_name.get(rtype, f'?{rtype}')
    instr = struct.unpack('>I', data[off:off+4])[0] if off + 4 <= size_ram else 0
    print(f'  raw=0x{r:08X}  type={name:11}  off=0x{off:06X}  instr_at_off=0x{instr:08X}')
