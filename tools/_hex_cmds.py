with open('F:/Projects/n64recomp/PokemonStadiumRecomp/audio_task_corrupt.bin','rb') as f:
    d = f.read()
for i in range(20, 30):
    off = i*8
    bs = ' '.join(f'{b:02X}' for b in d[off:off+8])
    print(f'cmd {i:>3}: bytes @ 0x{off:03X}: {bs}')
