"""Decode audio command list from audio_task_corrupt.bin.

Each cmd is 8 bytes: word0 (opcode + 24-bit param), word1 (32-bit operand).
Stadium dispatch: handler_pc = MEM_H[((word0 >> 23) & 0xFE)].
                  opcode_byte = (word0 >> 24) & 0xFF.
"""
import struct

DISPATCH = {
    0x01: 0x139C,
    0x02: 0x119C,
    0x03: 0x1A64,
    0x04: 0x11C8,
    0x05: 0x17EC,
    0x06: 0x1208,
    0x09: 0x127C,
    0x0A: 0x1348,
    0x0B: 0x1248,
    0x0C: 0x1C84,
    0x0D: 0x12D4,
    0x0E: 0x12B0,
    0x0F: 0x1384,
}

with open('F:/Projects/n64recomp/PokemonStadiumRecomp/audio_task_corrupt.bin', 'rb') as f:
    data = f.read()

n = len(data) // 8
print(f'Decoding {n} commands ({len(data)} bytes)')
print(f'{"idx":>3}  {"w0":>10}  {"w1":>10}  {"op":>4}  handler  notes')

for i in range(n):
    # rdram stores word-swapped BE -> LE-unpack gives the BE word the RSP sees.
    w0, w1 = struct.unpack('<II', data[i*8:i*8+8])
    op = (w0 >> 24) & 0xFF
    handler = DISPATCH.get(op, '???')
    handler_str = f'0x{handler:04X}' if isinstance(handler, int) else handler
    # Common decompositions
    low24 = w0 & 0xFFFFFF
    print(f'{i:>3}  {w0:08X}  {w1:08X}  {op:02X}  {handler_str}  low24=0x{low24:06X}')
