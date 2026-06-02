"""Inspect the in-flight white-screen freeze post-mortem capture.

Reports:
  - reason / status counters
  - Gfx Thread call stack (should end in RT64::Interpreter::processDisplayLists)
  - Game-thread state (should be in osRecvMesg waiting for VI)
  - Audio rings (should be clean if RT64 is the only thing stuck)
  - Freeze DL summary
"""
import json
import os
import sys

REPORT = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"
FREEZE_DL = "build/last_run_freeze_dl.bin"

d = json.load(open(REPORT))

print("=== reason / SEH ===")
print(f"  reason: {d.get('reason')}")
seh = d.get("seh") or {}
for k, v in seh.items():
    print(f"  seh.{k}: {v}")

print("\n=== status counters at freeze ===")
for k, v in d.get("status", {}).items():
    print(f"  {k}: {v}")
sg = d.get("status", {}).get("submit_gfx", 0)
dp = d.get("status", {}).get("dp_complete", 0)
print(f"  >>> submit_gfx - dp_complete = {sg - dp} (expect 1 if RT64 hang)")

print("\n=== threads of interest ===")
threads = d.get("threads", []) or []
print(f"  total threads: {len(threads)}")
INTEREST = ("Gfx Thread", "SP Task Thread", "RT64 Interpreter",
            "RT64 Workload", "RT64 Idle", "SDLAudioP2", "Audio")
for t in threads:
    name = t.get("name") or ""
    tid = t.get("tid", "?")
    stack = t.get("stack") or t.get("frames") or []
    if name in INTEREST or any(n in name for n in INTEREST):
        print(f"\n  --- tid={tid} name={name!r} (frames={len(stack)}) ---")
        for i, fr in enumerate(stack[:25]):
            sym = fr.get("name") or "?"
            file = fr.get("file") or ""
            line = fr.get("line", 0)
            pc = fr.get("pc", 0)
            print(f"     [{i:2d}] {sym}  ({file}:{line})  pc=0x{pc:X}")

print("\n=== game thread (look for osRecvMesg / WaitForSingleObject inside event loop) ===")
for t in threads:
    name = t.get("name") or ""
    stack = t.get("stack") or t.get("frames") or []
    if "Gfx" in name or "RT64" in name or "Audio" in name or "SP " in name:
        continue
    sym_top = (stack[0] if stack else {}).get("symbol", "")
    if any(s in str(sym_top) for s in ("osRecv", "WaitFor", "ConditionVariable", "Sleep")):
        print(f"\n  --- tid={t.get('tid')} name={name!r} ---")
        for i, fr in enumerate(stack[:25]):
            sym = fr.get("symbol") or "?"
            mod = fr.get("module") or ""
            pc = fr.get("pc_hex") or fr.get("pc") or ""
            print(f"     [{i:2d}] {sym}  {mod}  {pc}")

print("\n=== audio rings totals (should be clean) ===")
for r in ["audio_afx_in", "audio_afx_out", "audio_mbp_pre", "audio_mbp_disp",
          "audio_mbp_chain"]:
    e = d.get(r, {})
    seq = e.get("seq")
    cap = e.get("cap")
    print(f"  {r}: seq={seq} cap={cap}")

print(f"\n=== freeze DL ===")
if os.path.exists(FREEZE_DL):
    sz = os.path.getsize(FREEZE_DL)
    with open(FREEZE_DL, "rb") as f:
        head = f.read(64)
    print(f"  path: {FREEZE_DL}")
    print(f"  size: {sz} bytes (~{sz//8} commands at 8 bytes each)")
    print(f"  first 8 cmds (w0,w1):")
    for i in range(min(8, sz // 8)):
        w0 = int.from_bytes(head[i*8:i*8+4], "big")
        w1 = int.from_bytes(head[i*8+4:i*8+8], "big")
        op = (w0 >> 24) & 0xFF
        print(f"    [{i:2d}] w0=0x{w0:08X} w1=0x{w1:08X}  op=0x{op:02X}")
else:
    print(f"  {FREEZE_DL} not found")
