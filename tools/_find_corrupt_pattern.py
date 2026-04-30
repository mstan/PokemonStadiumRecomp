"""Search ROM for the bytes that ended up overwriting the SoundBank
at 0x8018A1B0. The synth-time dump at offset 0x00 was:
  33 CF EF 59 FF 59 FF 57 FF 55 FF 53 FF 51 84 49

If those bytes exist in ROM at offset X, then a DMA from ROM offset X
landed on top of the SoundBank's first 0x40 bytes.
"""
needle = bytes([0x33, 0xCF, 0xEF, 0x59, 0xFF, 0x59, 0xFF, 0x57,
                0xFF, 0x55, 0xFF, 0x53, 0xFF, 0x51, 0x84, 0x49])
with open('baserom.z64', 'rb') as f:
    data = f.read()
print(f'rom size: {len(data):,}')

hits = []
i = 0
while True:
    i = data.find(needle, i)
    if i < 0:
        break
    hits.append(i)
    i += 1
print(f'hits for full 16-byte pattern: {len(hits)}')
for h in hits[:10]:
    print(f'  rom 0x{h:08X}')

n8 = needle[:8]
i = 0
hits8 = []
while True:
    i = data.find(n8, i)
    if i < 0:
        break
    hits8.append(i)
    i += 1
print(f'hits for first 8 bytes (33 CF EF 59 FF 59 FF 57): {len(hits8)}')
for h in hits8[:10]:
    print(f'  rom 0x{h:08X}')

# Maybe ROM is byte-swapped versus what we dumped (XOR-3 in RDRAM).
# Try the BE-as-words pattern.
import struct
words = struct.unpack('>4I', needle)
print('words BE:', [hex(w) for w in words])
# And LE
words_le = struct.unpack('<4I', needle)
print('words LE:', [hex(w) for w in words_le])
