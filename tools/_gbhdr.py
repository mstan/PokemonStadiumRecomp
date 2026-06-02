import sys
d = open(sys.argv[1], 'rb').read()
ct = {0x00:'ROM ONLY',0x01:'MBC1',0x02:'MBC1+RAM',0x03:'MBC1+RAM+BAT',
      0x05:'MBC2',0x06:'MBC2+BAT',0x0F:'MBC3+TIMER+BAT',0x10:'MBC3+TIMER+RAM+BAT',
      0x11:'MBC3',0x12:'MBC3+RAM',0x13:'MBC3+RAM+BAT',
      0x19:'MBC5',0x1A:'MBC5+RAM',0x1B:'MBC5+RAM+BAT',
      0x1C:'MBC5+RUMBLE',0x1D:'MBC5+RUMBLE+RAM',0x1E:'MBC5+RUMBLE+RAM+BAT'}
print('size', len(d))
print('title', d[0x134:0x143].decode('ascii', 'replace').rstrip('\x00'))
print('CGB(0x143)', hex(d[0x143]))
print('cart type(0x147)', hex(d[0x147]), ct.get(d[0x147], '?'))
print('rom size(0x148)', hex(d[0x148]), '->', 32*(1 << d[0x148]), 'KB')
print('ram size(0x149)', hex(d[0x149]))
