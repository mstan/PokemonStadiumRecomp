"""Check fragment31's bytes at offset +8 — does it have FRAGMENT magic?"""
data = open("disasm/assets/us/fragments/31/fragment31_header.textbin.bin", "rb").read()
print(f"fragment31_header.textbin.bin size = {len(data)} bytes")
print("first 0x20 bytes:")
for i in range(0, min(0x20, len(data)), 4):
    chunk = data[i:i+4]
    hexs = " ".join(f"{b:02x}" for b in chunk)
    asc = "".join(chr(b) if 0x20 <= b < 0x7f else "." for b in chunk)
    print(f"  +0x{i:02x}: {hexs}  {asc}")
print()
# Check FRAGMENT magic at +8 (4 bytes "FRAG" + 4 bytes "MENT")
sig0 = int.from_bytes(data[8:12], "big")
sig1 = int.from_bytes(data[12:16], "big")
print(f"+0x08 word = {sig0:#010x} (expect 0x46524147 'FRAG')")
print(f"+0x0c word = {sig1:#010x} (expect 0x4D454E54 'MENT')")
print(f"FRAGMENT magic present: {sig0 == 0x46524147 and sig1 == 0x4D454E54}")
