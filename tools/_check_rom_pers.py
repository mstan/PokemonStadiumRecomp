"""Read ROM bytes at the offsets the lookup container points to,
check for PERS-SZP / PRES-* magic. Validates the data-structure
assumption that the container is a TOC for the pokemon_models
segment with each entry being (offset_within_blob, length)."""

ROM = "F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64"
BUFFER_BASE = 0x00920000  # container[+0x4]

# entries we peeked at runtime (offset, length):
ENTRIES = [
    (0x00000d80, 0x0000de90),
    (0x0000ec10, 0x00021140),
    (0x0002fd50, 0x00025090),
    (0x00054de0, 0x00018d40),
    (0x0006db20, 0x00013fa0),  # entry idx 5 — the one that misses
    (0x00081ac0, 0x0001cd60),
    (0x0009e820, 0x0000f530),
]

with open(ROM, "rb") as f:
    rom = f.read()

print(f"ROM size: 0x{len(rom):X}")
print(f"buffer_base in container: 0x{BUFFER_BASE:X}")
print()
print("Bytes at container's buffer_base (first 16):")
print(f"  ROM[0x{BUFFER_BASE:X}..]: {rom[BUFFER_BASE:BUFFER_BASE+16].hex()}")
print()

for i, (off, length) in enumerate(ENTRIES, start=1):
    rom_addr = BUFFER_BASE + off
    if rom_addr + 16 > len(rom):
        print(f"entry {i}: rom_addr=0x{rom_addr:X} OOB")
        continue
    head = rom[rom_addr:rom_addr+16]
    magic_ascii = ''.join(chr(b) if 0x20 <= b < 0x7F else '.' for b in head[:8])
    print(f"entry {i}: rom_addr=0x{rom_addr:X} len=0x{length:X}")
    print(f"  bytes: {head.hex()}")
    print(f"  ascii: '{magic_ascii}'")
    if head[:8] == b"PERS-SZP":
        print(f"  -> PERS-SZP wrapper")
    elif head[:4] == b"PRES":
        print(f"  -> PRES wrapper")
    else:
        print(f"  -> NOT a wrapper")
    print()
