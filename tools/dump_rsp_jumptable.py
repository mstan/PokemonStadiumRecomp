import struct, sys

with open('F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64', 'rb') as f:
    f.seek(0x7F670)
    data = f.read(0x2B0)

print("=== Data segment halfwords (DMEM offset : value) ===")
for i in range(0, len(data), 16):
    halfwords = ' '.join(f'{struct.unpack(">H", data[i+j:i+j+2])[0]:04X}' for j in range(0, 16, 2))
    print(f'{i:04X}: {halfwords}')

print()
print("=== Plausible IMEM jump targets (values 0x0000-0x0FFF, looks like (addr-0x1000)>>1 idx?) ===")
print("=== Looking for halfwords that could be IMEM targets (low halfwords < 0x1000) ===")
candidates = set()
for i in range(0, len(data) - 1, 2):
    v = struct.unpack(">H", data[i:i+2])[0]
    # Likely IMEM addresses are < 0x1000 (since IMEM is 0x1000 bytes)
    # The dispatcher does (jump_target | 0x1000) & 0x1FFF, so values 0x0000-0x0FFF map to 0x1000-0x1FFF
    if v < 0x1000 and v % 4 == 0 and v != 0:
        candidates.add(v | 0x1000)

print("Distinct candidates (sorted):")
for c in sorted(candidates):
    print(f'  0x{c:04X},')
