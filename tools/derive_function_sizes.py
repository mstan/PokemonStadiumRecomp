#!/usr/bin/env python3
"""
derive_function_sizes.py — compute sizes for zero-size FUNC symbols.

pret's hand-written assembly (hasm) functions land in the ELF as
`FUNC GLOBAL` symbols with size=0 because the assembler doesn't emit
.size directives for them. N64Recomp can't recompile them without
knowing the byte length, so any caller emits "No function found
for jal target" errors.

This script reads the ELF symbol table, sorts FUNC symbols by VRAM
within each section, and computes each zero-size symbol's implied
size as `(next_symbol_vram - this_vram)` capped at the section end.
Emits a TOML fragment of `[[input.function_sizes]]` entries, which
we splice into game.toml.

Usage:
  python3 tools/derive_function_sizes.py > tools/derived_function_sizes.toml

Then merge the output into game.toml's [[input.function_sizes]]
section.

This is NOT a stub: we are recovering real function sizes from the
ELF that pret already built — we're not inventing behavior. The same
data is observable via readelf;
N64Recomp just doesn't auto-derive it itself yet (that's the engine
fix to upstream).
"""

import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ELF = Path(__file__).resolve().parent.parent / "disasm" / "build" / "pokestadium-us.elf"
READELF = "mips-linux-gnu-readelf"  # in WSL


def run_readelf():
    """Use WSL to invoke the MIPS readelf and return raw output."""
    elf_wsl = "/mnt/f/Projects/PokemonStadiumRecomp/disasm/build/pokestadium-us.elf"
    out = subprocess.run(
        ["wsl", "-d", "Ubuntu", "--", "mips-linux-gnu-readelf", "-s", elf_wsl],
        capture_output=True, text=True, check=True,
    )
    return out.stdout


def parse_symbols(text):
    """Yield (vram, size, type, bind, vis, ndx, name) for every symbol."""
    for line in text.splitlines():
        # Format: "  N: VRAM SIZE TYPE BIND VIS NDX NAME"
        parts = line.split()
        if len(parts) < 8:
            continue
        try:
            vram = int(parts[1], 16)
            size = int(parts[2])
        except ValueError:
            continue
        sym_type = parts[3]
        bind = parts[4]
        vis = parts[5]
        ndx = parts[6]
        name = parts[7]
        yield vram, size, sym_type, bind, vis, ndx, name


def derive_sizes(symbols):
    """For each section, sort FUNC symbols by VRAM. For each zero-size
    one, compute its size as (next_symbol_vram - this_vram). The last
    zero-size symbol in a section can't be derived this way and is
    skipped (caller can fall back to a manual entry)."""

    # Group all symbols (any type) by section. We need ALL symbols
    # because a non-FUNC symbol may immediately follow a zero-size
    # FUNC, and that defines the FUNC's size.
    by_section = defaultdict(list)
    for sym in symbols:
        vram, size, sym_type, bind, vis, ndx, name = sym
        if ndx in ("ABS", "UND", "COM"):
            continue
        if vram == 0:
            continue
        by_section[ndx].append(sym)

    derived = []  # list of (name, size) for zero-size FUNCs
    skipped_last = []  # name of zero-size FUNCs that are last in section

    for ndx, syms in by_section.items():
        # Sort by VRAM, breaking ties by putting non-zero-size FUNCs
        # first so we don't pick a sibling zero-size FUNC as the
        # boundary.
        syms.sort(key=lambda s: (s[0], 0 if s[1] > 0 else 1))

        # Build a sorted list of distinct VRAMs occupied by ANY symbol
        # (for finding the next-VRAM boundary).
        boundaries = sorted({s[0] for s in syms})

        for vram, size, sym_type, bind, vis, _, name in syms:
            if sym_type != "FUNC" or size != 0:
                continue
            # Skip .L* (local labels — pret marks jump targets within
            # hand-written asm chunks; these are NOT separate functions
            # and shouldn't be auto-sized as if they were. Treating them
            # as functions causes the engine to emit "branch outside"
            # errors when the parent function branches across them.)
            if name.startswith(".L"):
                continue
            # Skip D_* (data symbols miscategorized as FUNC by pret).
            if name.startswith("D_"):
                continue
            # Find next boundary VRAM strictly greater than this one
            next_vram = None
            for b in boundaries:
                if b > vram:
                    next_vram = b
                    break
            if next_vram is None:
                skipped_last.append(name)
                continue
            derived_size = next_vram - vram
            if derived_size <= 0 or derived_size > 0x10000:
                # Sanity check: huge or non-positive sizes are wrong.
                # Skip; the caller can manually-size if needed.
                continue
            if derived_size & 3:
                # MIPS instructions are always 4 bytes. A non-aligned
                # size means the next "boundary" is mid-instruction
                # data — not a function boundary. Skip.
                continue
            derived.append((name, derived_size, vram))

    return derived, skipped_last


def emit_toml(derived):
    print("# Auto-generated by tools/derive_function_sizes.py")
    print("# Do not edit by hand — re-run the script to refresh.")
    print(f"# Derived sizes for {len(derived)} zero-size FUNC symbols")
    print(f"# (hasm functions whose ELF size=0 because the assembler")
    print(f"# doesn't emit .size for hand-written assembly).")
    print()
    for name, size, vram in derived:
        print(f"[[input.function_sizes]]")
        print(f'name = "{name}"')
        print(f"size = 0x{size:X}   # vram 0x{vram:08X}")
        print()


def main():
    text = run_readelf()
    syms = list(parse_symbols(text))
    derived, skipped_last = derive_sizes(syms)
    emit_toml(derived)
    if skipped_last:
        print(f"# {len(skipped_last)} zero-size FUNC symbols are the last in",
              file=sys.stderr)
        print(f"# their section and could not be auto-derived. They may",
              file=sys.stderr)
        print(f"# need manual entries:", file=sys.stderr)
        for name in skipped_last[:20]:
            print(f"#   {name}", file=sys.stderr)
        if len(skipped_last) > 20:
            print(f"#   ... ({len(skipped_last) - 20} more)", file=sys.stderr)


if __name__ == "__main__":
    main()
