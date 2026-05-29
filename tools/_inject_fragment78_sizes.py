"""Patch fragment78.s for N64Recomp consumption.

Fragment78 is unusual: only the entry trampoline `func_8FF00020` (12
instructions ending in `jr $ra`) is real code. Splat detected dozens of
other `glabel func_XXX` symbols inside the rest of the fragment, but the
bytes there are pure animation/model data — they happen to disassemble
to mostly-invalid MIPS, and N64Recomp chokes when trying to recompile
them as functions (e.g. "Failed to analyze func_8FF0C7B4" from a `lw`
with -3226 stack offset).

Fix: tell the assembler the truth about the fragment shape:

  1. `func_8FF00020` is a function with size 0x1C, ending right after
     its `jr $ra` delay slot at offset 0x3C of the fragment.
  2. Every other `glabel func_XXX` is data — convert it to `dlabel`
     so the ELF symbol type is NOTYPE rather than @function. N64Recomp
     skips NOTYPE symbols when populating section_functions[].

This lets fragment78 register as a section with one function (the
entry), and Stadium's runtime J trampoline at the fragment base
dispatches into it cleanly.

Run after every splat regen of asm/us/fragments/78/fragment78.s.
"""
import re
import sys

PATH = 'F:/Projects/n64recomp/PokemonStadiumRecomp/disasm/asm/us/fragments/78/fragment78.s'

GLABEL_RE = re.compile(r'^(\s*)glabel\s+(\S+)\s*$')
SIZE_RE = re.compile(r'^\.size\s+\S+,\s*\.\-\S+\s*$')
ENTRY_LABEL = 'func_8FF00020'
ENTRY_VRAM = 0x8FF00020
# Entry function ends at vram 0x8FF00038 (delay slot of jr $ra). Size
# directive is placed immediately after, at 0x8FF0003C.
ENTRY_END_OFFSET = 0x1C
INSTR_RE = re.compile(r'/\*\s*[0-9A-F]+\s+([0-9A-F]+)\s+[0-9A-F]+\s*\*/')

with open(PATH) as f:
    lines = f.readlines()

out = []
in_entry_fn = False
entry_size_emitted = False

for line in lines:
    # Drop any pre-existing .size directives from prior runs.
    if SIZE_RE.match(line):
        continue

    m = GLABEL_RE.match(line)
    if m:
        indent, label = m.groups()
        if label == ENTRY_LABEL:
            in_entry_fn = True
            out.append(line)
            continue
        # Convert every other glabel to dlabel.
        out.append(f'{indent}dlabel {label}\n')
        continue

    # While inside the entry function, watch for the instruction
    # immediately following the delay slot at 0x8FF00038. That is
    # vram 0x8FF0003C — emit the .size on the line above it.
    if in_entry_fn and not entry_size_emitted:
        instr_m = INSTR_RE.search(line)
        if instr_m:
            instr_vram = int(instr_m.group(1), 16)
            if instr_vram >= ENTRY_VRAM + ENTRY_END_OFFSET:
                out.append(f'.size {ENTRY_LABEL}, .-{ENTRY_LABEL}\n')
                entry_size_emitted = True
                in_entry_fn = False
    out.append(line)

# Edge case: if the entry function is the very last thing in the file
# (it isn't here, but stay defensive), close it out.
if in_entry_fn and not entry_size_emitted:
    out.append(f'.size {ENTRY_LABEL}, .-{ENTRY_LABEL}\n')

with open(PATH, 'w') as f:
    f.writelines(out)

print(f'patched {PATH}: kept {ENTRY_LABEL} as function, rest converted to dlabel')
