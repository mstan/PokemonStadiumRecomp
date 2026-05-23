#!/usr/bin/env python3
"""Scan EVERY fragment's reloc table for R_MIPS_32 entries whose
input literal is in the pattern bucket 0x8FF00000. Whichever
fragment has a reloc with input literal 0x8FF0ABFC (or any pattern-
bucket address) is the source of the runtime-observed literal.

Each fragment in the disassembly has assets/us/fragments/N/fragment_N_reloc.rodatabin.bin
(or similar) and a yaml entry with a ROM start. We use the rom.yaml
to enumerate fragments, then read each one's body + reloc table.
"""

import os
import struct
import yaml
import re
from pathlib import Path

ROM = Path("F:/Projects/n64recomp/PokemonStadiumRecomp/baserom.z64")
YAML = Path("F:/Projects/n64recomp/PokemonStadiumRecomp/disasm/yamls/us/rom.yaml")
ASSETS = Path("F:/Projects/n64recomp/PokemonStadiumRecomp/disasm/assets/us/fragments")

PATTERN_BUCKET = 0x8FF00000
PATTERN_BUCKET_MASK = 0xFFF00000

with open(ROM, "rb") as f:
    rom = f.read()

# Walk fragments/* dirs; each has fragment_<N>_reloc.rodatabin.bin
print("scanning all fragments for R_MIPS_32 relocs targeting 0x8FF00000+:")
print()

# Use rom.yaml to map each fragment to its body ROM range and reloc bin
with open(YAML, "r") as f:
    rom_yaml = yaml.safe_load(f)

fragments = []  # (name, body_rom, body_end, vram, reloc_bin)
segments = rom_yaml if isinstance(rom_yaml, list) else rom_yaml.get("segments", [])
# rom.yaml's top level is segments dict; the file uses `segments` field
if isinstance(rom_yaml, dict):
    segments = rom_yaml.get("segments", [])

# Actually let me just scan all reloc bins directly
for d in sorted(ASSETS.iterdir()):
    if not d.is_dir():
        continue
    name = d.name
    reloc_bin = None
    for f in d.iterdir():
        if f.name.endswith("_reloc.rodatabin.bin"):
            reloc_bin = f
            break
    if reloc_bin is None:
        continue
    fragments.append((name, reloc_bin))

# Need ROM range for each fragment; parse rom.yaml
rom_ranges = {}  # name -> (start, end, vram)
prev = None
for seg in segments:
    if isinstance(seg, dict) and "name" in seg:
        if prev is not None:
            prev_name, prev_start, prev_vram = prev
            # We don't know end; use start of next segment
            pass

# Simpler: parse rom.yaml lines directly
import re as re_mod
with open(YAML, "r") as f:
    text = f.read()
# Find fragmentN entries with start/vram
seg_re = re.compile(
    r"  - name: (fragment\d+)\s*\n"
    r"\s+type:[^\n]*\n"
    r"\s+bss_size:[^\n]*\n"
    r"\s+start: 0x([0-9A-Fa-f]+)\s*\n"
    r"\s+vram: 0x([0-9A-Fa-f]+)"
)
matches = list(seg_re.finditer(text))
all_segments = []
for m in matches:
    name = m.group(1)
    start = int(m.group(2), 16)
    vram = int(m.group(3), 16)
    all_segments.append((name, start, vram))
# Also any other segments to determine "end" (next segment's start)
# Just compute end as next segment's start
all_segments.sort(key=lambda x: x[1])
ends = {}
for i, (n, s, v) in enumerate(all_segments):
    if i + 1 < len(all_segments):
        ends[n] = all_segments[i + 1][1]
    else:
        ends[n] = len(rom)

# Now process each fragment N
total_pattern_relocs = 0
hot_fragments = []
for name, reloc_bin in fragments:
    if name not in [n for n, _, _ in all_segments]:
        continue
    seg = next(((n, s, v) for n, s, v in all_segments if n == name), None)
    if seg is None:
        continue
    n_, body_start, vram = seg
    body_end = ends[n_]
    body = rom[body_start:body_end]

    relocs = reloc_bin.read_bytes()
    if len(relocs) < 4:
        continue
    n_relocs = struct.unpack(">I", relocs[0:4])[0]
    expected = 4 + n_relocs * 4
    # If the reloc bin doesn't include the count header, n_relocs might be wrong
    if expected > len(relocs):
        # Try without header (treat whole file as entries)
        n_relocs = len(relocs) // 4
        offset = 0
    else:
        offset = 4

    pattern_count = 0
    samples = []
    for i in range(n_relocs):
        if offset + 4*i + 4 > len(relocs):
            break
        entry = struct.unpack(">I", relocs[offset + 4*i : offset + 4*i + 4])[0]
        rtype = (entry >> 24) & 0x7F
        body_off = entry & 0x00FFFFF
        if rtype != 2:
            continue
        if body_off + 4 > len(body):
            continue
        word = struct.unpack(">I", body[body_off:body_off+4])[0]
        if (word & PATTERN_BUCKET_MASK) == PATTERN_BUCKET:
            pattern_count += 1
            target_off = word & 0x000FFFFF
            samples.append((body_off, word, target_off))

    if pattern_count > 0:
        hot_fragments.append((name, pattern_count, samples))
        total_pattern_relocs += pattern_count

print(f"fragments with R_MIPS_32 → pattern bucket: {len(hot_fragments)}")
print(f"total R_MIPS_32 → pattern bucket entries: {total_pattern_relocs}")
print()
for name, count, samples in hot_fragments:
    print(f"  {name}: {count} pattern-bucket relocs")
    by_off = {}
    for body_off, word, target_off in samples:
        by_off.setdefault(target_off, []).append((body_off, word))
    print(f"    distinct target offsets: {len(by_off)}")
    for off in sorted(by_off.keys())[:20]:
        srcs = by_off[off]
        print(f"      offset=0x{off:05X} literal=0x{(PATTERN_BUCKET|off):08X} ({len(srcs)} srcs)")
    if len(by_off) > 20:
        print(f"      ... and {len(by_off) - 20} more")
