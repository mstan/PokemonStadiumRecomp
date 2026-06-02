"""Compare submit-time vs walk-time G_DL CALL target snapshots.

Verdict per (submit_seq, target_vaddr) pair:
  EQUAL+VALID_DL    — buffer was a sane DL at both times. Normal.
  EQUAL+NOT_DL      — Stadium emitted G_DL pointing at non-DL data
                      (hypothesis A: wrong-pointer bug)
  DIFFERENT         — buffer overwritten between submit and walk
                      (hypothesis B: submit-too-early race)
  NO_WALK           — submit recorded but walk never happened
                      (DL never reached this CALL — fine, RT64 hung earlier)
  NO_SUBMIT         — walk recorded but no matching submit
                      (probe miss or submit ring overflowed first)

A cmd-head "looks like a DL" if the first byte is a known F3DEX2 opcode.
"""
from __future__ import annotations
import json
import sys
from collections import defaultdict

REPORT = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"
d = json.load(open(REPORT))

# Known F3DEX2 / common GBI opcodes. Anything outside this set is
# evidence the head bytes are NOT a DL.
KNOWN_OPS = {
    0x00,  # G_NOOP
    0x01,  # G_VTX
    0x05, 0x06,  # G_TRI1, G_TRI2
    0xD7, 0xD9, 0xDA, 0xDB, 0xDC,  # G_TEXTURE, GEOMETRY, BRANCH_Z, MOVEWORD, MOVEMEM
    0xDE, 0xDF,  # G_DL, G_ENDDL
    0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7,
    0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8,
    0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
}

def head_to_bytes(h: str) -> bytes:
    return bytes.fromhex(h)

def looks_like_dl(head: bytes) -> bool:
    """Heuristic: at least the first 2 cmds (16 bytes = 2x w0/w1) start
    with a known F3DEX2 op. We require the FIRST byte to be a known op
    AND the cmd at +0x08 to also start with a known op (since DLs are
    8-byte cmd sequences). Reject if either fails.
    """
    if len(head) < 16:
        return False
    op0 = head[0]
    op1 = head[8]
    return op0 in KNOWN_OPS and op1 in KNOWN_OPS

def cmd_summary(head: bytes) -> str:
    """Show first 2 cmds for human reading."""
    if len(head) < 16:
        return "?"
    w0_a = int.from_bytes(head[0:4], "big")
    w1_a = int.from_bytes(head[4:8], "big")
    w0_b = int.from_bytes(head[8:12], "big")
    w1_b = int.from_bytes(head[12:16], "big")
    return f"w0a=0x{w0_a:08X} w1a=0x{w1_a:08X} | w0b=0x{w0_b:08X} w1b=0x{w1_b:08X}"

submit = d.get("gdl_submit", {})
walk   = d.get("gdl_walk", {})
print(f"gdl_submit: seq={submit.get('seq')} cap={submit.get('cap')} entries={len(submit.get('events', []))}")
print(f"gdl_walk:   seq={walk.get('seq')} cap={walk.get('cap')} entries={len(walk.get('events', []))}")

# Index walk events by (submit_seq, target). The walk side may have
# many entries per pair (RT64 walks the same CALL multiple times if
# the parent DL has multiple G_DLs or is itself called repeatedly).
# For diagnosis we want the MOST RECENT walk per (submit_seq, target),
# since that's what was on screen at freeze time.
walk_by_key = {}
for e in walk.get("events", []):
    key = (e["submit_seq"], e["target"])
    walk_by_key[key] = e  # keep last

# Same indexing for submit side.
submit_by_key = {}
for e in submit.get("events", []):
    key = (e["submit_seq"], e["target"])
    submit_by_key[key] = e

# Classify each pair.
classifications = defaultdict(list)
for key, sub in submit_by_key.items():
    walk_ev = walk_by_key.get(key)
    sub_head = head_to_bytes(sub["head"])
    if walk_ev is None:
        classifications["NO_WALK"].append((key, sub, None))
        continue
    walk_head = head_to_bytes(walk_ev["head"])
    if sub_head == walk_head:
        if looks_like_dl(walk_head):
            classifications["EQUAL+VALID_DL"].append((key, sub, walk_ev))
        else:
            classifications["EQUAL+NOT_DL"].append((key, sub, walk_ev))
    else:
        classifications["DIFFERENT"].append((key, sub, walk_ev))

# Walks without submits.
for key, w in walk_by_key.items():
    if key not in submit_by_key:
        classifications["NO_SUBMIT"].append((key, None, w))

print(f"\n=== classification counts ===")
for cls, items in classifications.items():
    print(f"  {cls}: {len(items)}")

# Detail dump for the interesting categories.
INTERESTING = ["EQUAL+NOT_DL", "DIFFERENT", "NO_SUBMIT"]
for cls in INTERESTING:
    items = classifications.get(cls, [])
    if not items:
        continue
    print(f"\n=== {cls} ({len(items)} pairs) ===")
    # Show last 10 (most recent submit_seq).
    items.sort(key=lambda x: (x[0][0], x[0][1]))
    for (submit_seq, target_vaddr), sub, w in items[-10:]:
        print(f"\n  submit_seq={submit_seq} target=0x{target_vaddr:08X}")
        if sub:
            sub_head = head_to_bytes(sub["head"])
            print(f"    submit  parent=0x{sub['parent']:08X}  {cmd_summary(sub_head)}")
        if w:
            walk_head = head_to_bytes(w["head"])
            print(f"    walk    parent=0x{w['parent']:08X}  {cmd_summary(walk_head)}")

# Final verdict.
print(f"\n=== VERDICT ===")
n_not_dl = len(classifications.get("EQUAL+NOT_DL", []))
n_diff = len(classifications.get("DIFFERENT", []))
n_valid = len(classifications.get("EQUAL+VALID_DL", []))
if n_not_dl > 0 and n_diff == 0:
    print(f"  Hypothesis A confirmed: {n_not_dl} CALL targets had non-DL bytes "
          f"at BOTH submit and walk time. Stadium emits G_DL into wrong buffers.")
elif n_diff > 0 and n_not_dl == 0:
    print(f"  Hypothesis B confirmed: {n_diff} CALL targets had bytes that "
          f"CHANGED between submit and walk. Stadium overwrites buffers mid-frame.")
elif n_diff > 0 and n_not_dl > 0:
    print(f"  MIXED: {n_not_dl} wrong-pointer + {n_diff} race. Both bugs present.")
else:
    print(f"  All {n_valid} CALL targets were valid DLs at both times. Bug is "
          f"not at the top-level CALL boundary. Look one level deeper "
          f"(walk-time of G_DL push=0 BRANCHes, or sub-DL contents).")
