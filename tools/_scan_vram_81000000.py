"""Scan ROM for PERS-SZP/Yay0 wrappers that decompress to fragments
at vram 0x81000000 (link bucket 0x10). Stadium hit a runtime lookup
miss at vram 0x810001D0 after the 0x8FF00000 pattern was activated —
0x81000000 is the next dynamic-asset slot to handle.
"""

from collections import Counter
import hashlib

ROM = "F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64"


def yay0_decompress(data, target_size=None):
    if data[:4] != b"Yay0":
        return None
    decomp_size = int.from_bytes(data[4:8], "big")
    link_off = int.from_bytes(data[8:12], "big")
    data_off = int.from_bytes(data[12:16], "big")
    if target_size is None:
        target_size = decomp_size
    out = bytearray(target_size + 32)
    out_pos = 0
    cmd_pos = 16
    link_pos = link_off
    data_pos = data_off
    cmd = 0
    cmd_bits = 0
    try:
        while out_pos < target_size:
            if cmd_bits == 0:
                cmd = int.from_bytes(data[cmd_pos:cmd_pos + 4], "big")
                cmd_pos += 4
                cmd_bits = 32
            if cmd & 0x80000000:
                out[out_pos] = data[data_pos]
                out_pos += 1
                data_pos += 1
            else:
                link = int.from_bytes(data[link_pos:link_pos + 2], "big")
                link_pos += 2
                offset = link & 0x0FFF
                count = link >> 12
                if count == 0:
                    count = data[data_pos] + 0x12
                    data_pos += 1
                else:
                    count += 2
                ref = out_pos - offset - 1
                for _ in range(count):
                    if out_pos >= len(out):
                        return None
                    out[out_pos] = out[ref]
                    out_pos += 1
                    ref += 1
            cmd <<= 1
            cmd_bits -= 1
    except IndexError:
        return None
    return bytes(out[:target_size]), decomp_size


with open(ROM, "rb") as f:
    rom = f.read()

# J target = vram + 0x20 = 0x81000020
# J encoding: opcode 2 << 26 | (target >> 2) & 0x03FFFFFF
# target_field = 0x81000020 >> 2 = 0x20400008
# target_field & 0x03FFFFFF = 0x00400008 (since high bits are 0x80000000 → masked to 0)
# So J = 0x08000000 | 0x00400008 = 0x08400008
# BE bytes: 08 40 00 08
target_first8 = b"\x08\x40\x00\x08\x00\x00\x00\x00"
target_magic = b"FRAGMENT"

candidates = []
i = 0
while True:
    j = rom.find(b"Yay0", i)
    if j == -1:
        break
    res = yay0_decompress(rom[j : j + 0x80000], target_size=0x40)
    if res is not None:
        prefix, full_size = res
        if (len(prefix) >= 0x10
                and prefix[0:8] == target_first8
                and prefix[8:16] == target_magic):
            wrapper_off = j
            wrapper_format = "yay0"
            if j >= 0x18 and rom[j - 0x18:j - 0x18 + 8] == b"PERS-SZP":
                wrapper_off = j - 0x18
                wrapper_format = "pers_szp_yay0"
            candidates.append((wrapper_off, wrapper_format, j, full_size))
    i = j + 1

print(f"Total fragments at vram 0x81000000: {len(candidates)}")
print()

if not candidates:
    print("No fragments at vram 0x81000000 found via PERS-SZP/Yay0 scan.")
    print("This vram slot may use a different wrapper format, or the")
    print("fragment is reachable via a non-pattern path.")
    raise SystemExit(0)

# Decompress each and gather metadata
records = []
for wrap_off, wrap_fmt, yay0_off, full_size in candidates:
    res = yay0_decompress(rom[yay0_off : yay0_off + 0x100000])
    if res is None:
        continue
    body, _ = res
    if len(body) < 0x20:
        continue
    reloc_off = int.from_bytes(body[0x14:0x18], "big")
    size_ram = int.from_bytes(body[0x1C:0x20], "big")
    body_hash = hashlib.sha256(body).hexdigest()[:16]
    records.append({
        "wrap_off": wrap_off,
        "wrap_fmt": wrap_fmt,
        "reloc_off": reloc_off,
        "size_ram": size_ram,
        "body_hash": body_hash,
        "decomp_total_size": full_size,
        "body": body,
    })

print(f"Decompressed {len(records)} wrappers fully.")
print()
print("First 10 wrappers (rom, format, decomp_size, relocOffset, sizeInRam):")
for r in records[:10]:
    print(f"  ROM 0x{r['wrap_off']:08X} {r['wrap_fmt']:<14} "
          f"decomp=0x{r['decomp_total_size']:X} "
          f"relocOff=0x{r['reloc_off']:X} sizeInRam=0x{r['size_ram']:X}")

print()
total = sum(r["size_ram"] for r in records)
print(f"Total decompressed payload: {total / 1024 / 1024:.2f} MB ({total} bytes)")
print(f"Distinct contents: {len(set(r['body_hash'] for r in records))}")

# Look at the first wrapper's body around offset 0x1D0 (where the
# runtime lookup miss happened)
if records and len(records[0]['body']) > 0x1F0:
    print()
    print("First wrapper's bytes around offset 0x1D0 (the runtime lookup-miss "
          "address relative to vram 0x81000000):")
    body = records[0]["body"]
    for off in range(0x1B0, 0x1F0, 4):
        if off + 4 > len(body):
            break
        word = int.from_bytes(body[off:off + 4], "big")
        print(f"  +0x{off:05X}: 0x{word:08X}")
