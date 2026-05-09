"""Audit generated C for literals that fall inside any relocatable section's
link-time VRAM range. Such literals are smoking-gun candidates for the
white-screen RT64 bug class (project_white_screen_root_cause_2026_05_03 and
the 2026-05-08 fragment8 attract-stall regression).

A correctly-emitted reference is wrapped in RELOC_HI16/RELOC_LO16 macros that
resolve to section_addresses[idx] + offset at runtime. A literal hex constant
in the same VRAM range is a candidate failure: at runtime the section may be
loaded at a different base, and the literal will read into wrong memory.

Usage:
  python tools/audit_relocatable_literals.py [--show N]

Outputs:
  - count of suspicious literals per generated/*.c file
  - top N hits with file:line and a snippet
  - ranked summary by which section's VRAM was hit
"""
from __future__ import annotations

import argparse
import os
import re
import sys
from pathlib import Path
from typing import NamedTuple


PROJECT = Path(__file__).resolve().parent.parent
GENERATED = PROJECT / "generated"
OVERLAYS_INL = GENERATED / "recomp_overlays.inl"
RELOC_LIST = PROJECT / "fragments_relocatable.txt"


class Section(NamedTuple):
    index: int
    name: str
    ram_addr: int
    size: int

    @property
    def end(self) -> int:
        return self.ram_addr + self.size


# Regex for SectionTableEntry rows in recomp_overlays.inl.
# Each row gives ram_addr, size, and the funcs symbol — use that to derive a
# section name like "section_NNN_<name>_funcs" → "<name>".
SECTION_ROW_RE = re.compile(
    r"\.rom_addr\s*=\s*0x([0-9A-Fa-f]+)\s*,\s*"
    r"\.ram_addr\s*=\s*0x([0-9A-Fa-f]+)\s*,\s*"
    r"\.size\s*=\s*0x([0-9A-Fa-f]+)\s*,\s*"
    r"\.funcs\s*=\s*section_(\d+)_([A-Za-z0-9_]+)_funcs"
)


def parse_section_table() -> list[Section]:
    """Return every section's (index, name, ram_addr, size)."""
    text = OVERLAYS_INL.read_text(encoding="utf-8", errors="ignore")
    sections: list[Section] = []
    for m in SECTION_ROW_RE.finditer(text):
        rom_addr = int(m.group(1), 16)
        ram_addr = int(m.group(2), 16)
        size = int(m.group(3), 16)
        idx = int(m.group(4))
        name = m.group(5)
        sections.append(Section(index=idx, name=name, ram_addr=ram_addr, size=size))
    return sections


def parse_relocatable_set() -> set[str]:
    """Names listed in fragments_relocatable.txt (with leading '.' stripped)."""
    names: set[str] = set()
    for line in RELOC_LIST.read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("."):
            line = line[1:]
        names.add(line)
    return names


# Match 8-digit hex literals like 0x801A0390 or 0x8FF00000 in C source. Be
# conservative: only flag full 32-bit values; skip 16-bit ones that fit in a
# RELOC_HI16/LO16 immediate.
LITERAL_RE = re.compile(r"\b0[xX]([0-9A-Fa-f]{8})\b")

# Lines containing RELOC macros — those literals are correctly handled.
RELOC_LINE_RE = re.compile(r"\bRELOC_(HI16|LO16|JTBL_2A|UNHANDLED)")


def in_any_section(value: int, sections: list[Section]) -> Section | None:
    for s in sections:
        if s.ram_addr <= value < s.end:
            return s
    return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--show", type=int, default=20,
                    help="how many top hits to print (default 20)")
    ap.add_argument("--file", type=str, default=None,
                    help="restrict to a single generated C filename (relative to generated/)")
    args = ap.parse_args()

    all_sections = parse_section_table()
    reloc_names = parse_relocatable_set()
    # Filter: only fragments that are in the relocatable list count.
    # Fragment names in section_table appear as "fragment8", "fragment62", etc.
    reloc_sections = [s for s in all_sections
                      if s.name in reloc_names or s.name.startswith("fragment")]
    # De-duplicate by ram_addr range (placeholder VRAMs may collide).
    # Sort by ram_addr for fast bisect later.
    reloc_sections.sort(key=lambda s: s.ram_addr)
    print(f"Parsed {len(all_sections)} total sections; "
          f"{len(reloc_sections)} relocatable sections in scope.")
    if not reloc_sections:
        print("ERROR: no relocatable sections matched. Check fragments_relocatable.txt.")
        return 2

    # Print summary of ranges we'll flag against.
    range_lo = min(s.ram_addr for s in reloc_sections)
    range_hi = max(s.end for s in reloc_sections)
    print(f"Relocatable VRAM ranges span 0x{range_lo:08X}..0x{range_hi:08X}")
    print()

    # Walk generated/*.c (and *.inl) and scan literals.
    targets = sorted(GENERATED.glob("*.c"))
    if args.file:
        targets = [GENERATED / args.file]
    print(f"Scanning {len(targets)} generated C files...")

    total_hits = 0
    per_file_hits: dict[str, int] = {}
    per_section_hits: dict[str, int] = {}
    examples: list[tuple[str, int, str, Section, int]] = []

    for path in targets:
        try:
            content = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        rel = path.name
        for lineno, line in enumerate(content.splitlines(), 1):
            stripped = line.lstrip()
            if stripped.startswith("//") or stripped.startswith("*"):
                continue  # comment-only line — disasm annotation, not real code
            if RELOC_LINE_RE.search(line):
                continue  # line is a properly-emitted reloc — skip
            # Strip any trailing comment so a literal in `// 0x82304224:` doesn't
            # falsely flag the line.
            cmt = line.find("//")
            if cmt != -1:
                line = line[:cmt]
            # Skip diagnostic/debug-print contexts — they pass PC bounds as
            # arguments but never load through the literals.
            if "switch_error(" in line:
                continue
            if "recomp_unhandled_" in line:
                continue
            if "TRACE_ENTRY(" in line or "TRACE_RETURN(" in line:
                continue
            for m in LITERAL_RE.finditer(line):
                v = int(m.group(1), 16)
                # Cheap bounds filter
                if v < range_lo or v >= range_hi:
                    continue
                s = in_any_section(v, reloc_sections)
                if s is None:
                    continue
                total_hits += 1
                per_file_hits[rel] = per_file_hits.get(rel, 0) + 1
                per_section_hits[s.name] = per_section_hits.get(s.name, 0) + 1
                examples.append((rel, lineno, line.strip(), s, v))

    print(f"\nTotal suspicious literals: {total_hits}")
    print(f"Files with hits: {len(per_file_hits)}")
    print()

    # Top files by hit count.
    top_files = sorted(per_file_hits.items(), key=lambda x: -x[1])[:25]
    print("Top files:")
    for f, n in top_files:
        print(f"  {n:>5}  {f}")
    print()

    # Sections most often referenced as bare literals.
    top_sections = sorted(per_section_hits.items(), key=lambda x: -x[1])[:25]
    print("Sections most-referenced as bare literals:")
    for sec_name, n in top_sections:
        # Find that section's range for context
        srange = next((s for s in reloc_sections if s.name == sec_name), None)
        if srange:
            print(f"  {n:>5}  {sec_name:>30}  "
                  f"vram 0x{srange.ram_addr:08X}..0x{srange.end:08X}")
        else:
            print(f"  {n:>5}  {sec_name}")
    print()

    # First N example hits.
    print(f"First {args.show} example hits:")
    for rel, lineno, snippet, sec, v in examples[: args.show]:
        if len(snippet) > 140:
            snippet = snippet[:140] + "..."
        print(f"  {rel}:{lineno}  v=0x{v:08X} -> {sec.name}: {snippet}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
