"""build_aspmain_combined.py — generate aspmain_combined.bin

Stadium's aspMain ucode is loaded into RSP IMEM at offset 0x80 (NOT 0x0)
because rspboot occupies IMEM[0..0x80] first and DMAs aspMain text into
IMEM[0x80..0x1080].

For RSPRecomp to decode the absolute J/JAL targets in aspMain correctly
(e.g. `j 0x10EC` referencing the rspboot DMA-pump tail at IMEM[0xEC]),
both rspboot bytes AND aspMain bytes must be recompiled as one contiguous
function. The recompiler reads from a single ROM file at a single offset;
the simplest way to give it the combined image is to write a synthetic
binary that mirrors what rspboot+aspMain look like in IMEM at runtime.

Layout of `aspmain_combined.bin` (0x1080 bytes):
  [0x000..0x080)  rspboot bytes from ROM[0x66BC0..0x66C40]
                  (these match IMEM[0..0x80] post-rspboot-self-load)
  [0x080..0x1080) aspMain bytes from ROM[0x68020..0x69020]
                  (these match IMEM[0x80..0x1080] post-aspMain-DMA)

Run from anywhere that has the ROM at the relative path below, or pass
`--rom path` to override.

Why ROM[0x66BC0] and not ROM[0xB70]? Both ROM ranges contain identical
rspboot bytes (libultra includes rspboot twice). 0x66BC0 is contiguous
with the audio assets, which is incidental but easier to remember.
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


# Verified ROM (Stadium NTSC v1.0).
EXPECTED_MD5 = "ed1378bc12115f71209a77844965ba50"

RSPBOOT_ROM_OFFSET = 0x66BC0
RSPBOOT_SIZE       = 0x80
ASPMAIN_ROM_OFFSET = 0x68020
ASPMAIN_SIZE       = 0x1000


def main(argv: list[str]) -> int:
    here = Path(__file__).resolve().parent.parent
    ap = argparse.ArgumentParser()
    ap.add_argument("--rom", default=str(here / "baserom.z64"),
                    help="Path to baserom.z64 (Stadium NTSC v1.0)")
    ap.add_argument("--out", default=str(here / "aspmain_combined.bin"),
                    help="Output path for the combined binary")
    args = ap.parse_args(argv)

    rom = Path(args.rom).read_bytes()
    md5 = hashlib.md5(rom).hexdigest()
    if md5 != EXPECTED_MD5:
        print(f"ERROR: ROM md5 mismatch. Expected {EXPECTED_MD5}, got {md5}",
              file=sys.stderr)
        return 1

    rspboot = rom[RSPBOOT_ROM_OFFSET : RSPBOOT_ROM_OFFSET + RSPBOOT_SIZE]
    aspmain = rom[ASPMAIN_ROM_OFFSET : ASPMAIN_ROM_OFFSET + ASPMAIN_SIZE]
    combined = rspboot + aspmain
    assert len(combined) == RSPBOOT_SIZE + ASPMAIN_SIZE

    Path(args.out).write_bytes(combined)
    print(f"Wrote {args.out} ({len(combined)} bytes)")
    print(f"  rspboot: 0x{RSPBOOT_SIZE:X} bytes from ROM 0x{RSPBOOT_ROM_OFFSET:X}")
    print(f"  aspMain: 0x{ASPMAIN_SIZE:X} bytes from ROM 0x{ASPMAIN_ROM_OFFSET:X}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
