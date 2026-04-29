"""Check the bytes at ROM 0xE38 — the value the copy-protection
check compares against (must equal 0x828A in low 16 bits)."""
with open("F:/Projects/PokemonStadiumRecomp/baserom.z64", "rb") as f:
    rom = f.read()
val = int.from_bytes(rom[0xE38:0xE3C], "big")
print(f"ROM[0xE38..0xE3C] big-endian word: 0x{val:08X}")
print(f"low 16 bits: 0x{val & 0xFFFF:04X}")
print(f"expected:    0x828A")
print(f"match: {(val & 0xFFFF) == 0x828A}")
