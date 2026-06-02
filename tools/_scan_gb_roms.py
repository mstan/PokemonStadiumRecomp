"""Scan the Stadium ROM for embedded Game Boy ROM signatures.
Every GB ROM has the Nintendo logo at 0x104 (CE ED 66 66 CC 0D ...).
If GB games were built into Stadium, we'd see many copies. The emulator
may carry ONE reference copy for logo verification."""
import re

logo = bytes.fromhex('ceed6666cc0d000b03730083')
data = open('disasm/baseroms/us/baserom.z64', 'rb').read()
hits = [m.start() for m in re.finditer(re.escape(logo), data)]
print('Stadium ROM size', len(data))
print('GB Nintendo-logo occurrences in Stadium ROM:', len(hits))
for h in hits[:12]:
    print('  at 0x%X' % h)

yd = open('pokemon-yellow.gbc', 'rb').read()
yh = [m.start() for m in re.finditer(re.escape(logo), yd)]
print('GB logo occurrences in Yellow cart:', len(yh), 'at', [hex(x) for x in yh])

# Also: search Stadium ROM for GB cart title strings of known games
for title in (b'POKEMON YELLOW', b'POKEMON RED', b'POKEMON BLUE', b'POKEMON_GLDAAUE', b'PM_CRYSTAL'):
    c = data.count(title)
    if c:
        print('title %r found %d time(s) in Stadium ROM' % (title, c))
