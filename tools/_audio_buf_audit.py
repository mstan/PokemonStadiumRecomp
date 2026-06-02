"""Audit audio_task_corrupt.bin for malformed cmds.

Scan every cmd; for each, check that low24/low12/low16 fields fit the
shape of the n-audio variant ABI. Report cmds whose params are out of
range for any common interpretation. Group by voice to see whether
malformed cmds cluster.

n-audio cmd shapes (from disasm/src/libnaudio/n_abi.h):
  0x01 ADPCMdec:  w0_low24 = state phys (in pool), w1 packed
  0x02 CLEARBUFF: w0_low24 = dmem (12-bit really), w1 = count
  0x03 ENVMIXER:  packed
  0x04 LOADBUFF:  w0[12..23]=count (12-bit), w0[0..11]=dmem (12-bit), w1=ram phys
  0x05 RESAMPLE:  w0_low24 = state phys, w1 packed
  0x06 SAVEBUFF:  same shape as LOADBUFF
  0x09 (env state): w0_low24 = ?, w1 = small s16 packed
  0x0B LOADADPCM: w0_low24 = bookSize, w1 = book phys (in pool)
  0x0C: ?
  0x0D: ?
  0x0E: ?
  0x0F: ?
"""
import struct

DISPATCH = {
    0x01: 'ADPCMdec', 0x02: 'CLEARBUFF', 0x03: 'ENVMIXER',
    0x04: 'LOADBUFF', 0x05: 'RESAMPLE', 0x06: 'SAVEBUFF',
    0x09: 'env_state?', 0x0A: 'op0A?', 0x0B: 'LOADADPCM',
    0x0C: 'op0C?', 0x0D: 'op0D?', 0x0E: 'op0E?', 0x0F: 'op0F?',
}

POOL_LO = 0x0
POOL_HI = 0x800000  # 8MB usable RAM

def looks_like_phys_addr_in_pool(v):
    return POOL_LO <= v <= POOL_HI

def shape_check(idx, w0, w1):
    op = (w0 >> 24) & 0xFF
    name = DISPATCH.get(op, f'op{op:02X}?')
    notes = []
    if op == 0x02:  # CLEARBUFF: low24 = dmem (should be small), w1 = count (small)
        d_lo24 = w0 & 0xFFFFFF
        c = w1
        # Effective dmem on RSP = (d_lo16) + 0x4F0; should fit in DMEM
        d_lo16 = w0 & 0xFFFF
        c_lo16 = w1 & 0xFFFF
        if d_lo16 + 0x4F0 + c_lo16 > 0x1000:
            notes.append(f'OOB: dmem_eff=0x{d_lo16+0x4F0:X} count=0x{c_lo16:X} wraps')
        if d_lo24 > 0x10000 or c > 0x10000:
            notes.append(f'BAD24: d_lo24=0x{d_lo24:X} c=0x{c:X}')
    elif op == 0x04:  # LOADBUFF
        count = (w0 >> 12) & 0xFFF
        dmem = w0 & 0xFFF
        if not looks_like_phys_addr_in_pool(w1):
            notes.append(f'BAD_RAM: 0x{w1:X}')
    elif op == 0x0B:  # LOADADPCM
        if not looks_like_phys_addr_in_pool(w1):
            notes.append(f'BAD_BOOK_RAM: 0x{w1:X}')
    elif op == 0x01:  # ADPCMdec
        if not looks_like_phys_addr_in_pool(w0 & 0xFFFFFF):
            notes.append(f'BAD_STATE: 0x{w0&0xFFFFFF:X}')
    elif op == 0x05:  # RESAMPLE
        if not looks_like_phys_addr_in_pool(w0 & 0xFFFFFF):
            notes.append(f'BAD_STATE: 0x{w0&0xFFFFFF:X}')
    elif op == 0x03:  # ENVMIXER
        # w1 = state phys
        if not looks_like_phys_addr_in_pool(w1):
            notes.append(f'BAD_STATE: 0x{w1:X}')
    return name, notes

with open('F:/Projects/n64recomp/PokemonStadiumRecomp/audio_task_corrupt.bin', 'rb') as f:
    data = f.read()

n = len(data) // 8
print(f'Auditing {n} cmds')

bad_cnt = 0
voice_idx = -1
in_voice = False
for i in range(n):
    w0, w1 = struct.unpack('<II', data[i*8:i*8+8])
    op = (w0 >> 24) & 0xFF
    name, notes = shape_check(i, w0, w1)
    # Track voice boundaries: 0x0B (LOADADPCM) starts a new voice
    if op == 0x0B:
        voice_idx += 1
        in_voice = True
    flag = '!' if notes else ' '
    note_str = ' '.join(notes) if notes else ''
    if notes or op in (0x02,):
        bad_cnt += 1
        print(f'{flag} idx={i:>3} v={voice_idx:>2}  w0={w0:08X} w1={w1:08X} op={op:02X}({name})  {note_str}')

print(f'\n{bad_cnt} flagged cmds across {voice_idx+1} voices')
