"""Validate Shape A's runtime-content-identification assumption for the
279 fragments at vram 0x8FF00000.

Q1: Is (relocOffset, sizeInRam) unique across all 279?
Q2: If not unique, is (sizeInRam, byte_at_offset_0x20) unique?
Q3: If still not, what hash window IS unique?
Q4: How many fragments share identical decompressed bytes (true dups)?
Q5: Total decompressed-payload size.
"""

from collections import Counter
import hashlib

ROM = "F:/Projects/PokemonStadiumRecomp/baserom.z64"


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

target_first8 = b"\x0B\xFC\x00\x08\x00\x00\x00\x00"
target_magic = b"FRAGMENT"

candidates = []
i = 0
while True:
    j = rom.find(b"Yay0", i)
    if j == -1:
        break
    # Give the decoder a generous window — Yay0 link/data streams can
    # live well past 0x400. Use 0x80000 so we always fit the whole
    # wrapper for any fragment up to ~512KB.
    res = yay0_decompress(rom[j:j + 0x80000], target_size=0x40)
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

print(f"Total fragments at vram 0x8FF00000: {len(candidates)}")
print()

records = []
for c_idx, (wrap_off, wrap_fmt, yay0_off, full_size) in enumerate(candidates):
    res = yay0_decompress(rom[yay0_off:yay0_off + 0x80000])
    if res is None:
        print(f"  WARN: failed to decompress wrapper at 0x{wrap_off:X}")
        continue
    body, ds = res
    if len(body) < 0x20:
        continue
    reloc_off = int.from_bytes(body[0x14:0x18], "big")
    size_ram = int.from_bytes(body[0x1C:0x20], "big")
    first40 = body[:0x40]
    body_hash = hashlib.sha256(body).hexdigest()[:16]
    records.append({
        "wrap_off": wrap_off,
        "wrap_fmt": wrap_fmt,
        "reloc_off": reloc_off,
        "size_ram": size_ram,
        "body": body,
        "body_hash": body_hash,
        "first40": first40,
        "decomp_total_size": ds,
    })

print(f"Decompressed {len(records)} wrappers fully.")
print()

keys = [(r["reloc_off"], r["size_ram"]) for r in records]
key_counts = Counter(keys)
n_unique = sum(1 for v in key_counts.values() if v == 1)
n_dup = len(keys) - n_unique
print("Q1: (relocOffset, sizeInRam) discriminator")
print(f"    unique:    {n_unique}/{len(keys)}")
print(f"    in collision groups: {n_dup} fragments / "
      f"{len([v for v in key_counts.values() if v > 1])} groups")

keys2 = [(r["size_ram"], r["first40"][0x20:0x24]) for r in records]
kc2 = Counter(keys2)
n_unique2 = sum(1 for v in kc2.values() if v == 1)
print()
print("Q2: (sizeInRam, word_at_+0x20) discriminator")
print(f"    unique:    {n_unique2}/{len(keys2)}")

keys3 = [r["first40"] for r in records]
kc3 = Counter(keys3)
n_unique3 = sum(1 for v in kc3.values() if v == 1)
print()
print("Q3: first 0x40 bytes discriminator")
print(f"    unique:    {n_unique3}/{len(keys3)}")

keys3b = [r["body"][:0x100] for r in records]
kc3b = Counter(keys3b)
n_unique3b = sum(1 for v in kc3b.values() if v == 1)
print(f"Q3b: first 0x100 bytes: {n_unique3b}/{len(keys3b)} unique")

keys4 = [r["body_hash"] for r in records]
kc4 = Counter(keys4)
n_unique4 = sum(1 for v in kc4.values() if v == 1)
n_dup4 = len(keys4) - n_unique4
print()
print('Q4: full-body hash ("are any decompressed outputs identical")')
print(f"    unique:    {n_unique4}/{len(keys4)}")
print(f"    duplicate-content groups: "
      f"{len([v for v in kc4.values() if v > 1])} "
      f"({n_dup4} fragments are duplicates)")
print(f"    Distinct fragment contents: {len(kc4)}")

total = sum(r["size_ram"] for r in records)
print()
print("Q5: Decompressed-payload size")
print(f"    Total: {total / 1024 / 1024:.2f} MB ({total} bytes)")
print(f"    Avg per fragment: {total / len(records) / 1024:.1f} KB")
print(f"    Min size: 0x{min(r['size_ram'] for r in records):X}")
print(f"    Max size: 0x{max(r['size_ram'] for r in records):X}")

print()
print("Q6: drill into (reloc_off, size_ram) collisions")
shown = 0
for k, v in sorted(key_counts.items(), key=lambda x: -x[1]):
    if v > 1 and shown < 10:
        members = [r for r in records
                   if (r["reloc_off"], r["size_ram"]) == k]
        hashes = set(m["body_hash"] for m in members)
        wrap_offs = sorted(m["wrap_off"] for m in members)
        print(f"    reloc=0x{k[0]:X} size=0x{k[1]:X}: "
              f"{v} wrappers, {len(hashes)} distinct contents")
        if v <= 5:
            for w in wrap_offs:
                print(f"        ROM 0x{w:X}")
        shown += 1
