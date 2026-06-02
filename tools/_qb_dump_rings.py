"""Dump the raw audio ring tails from last_run_report.json."""
import json
import sys

d = json.load(open(sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"))
for r in ["audio_afx_in", "audio_afx_out", "audio_mbp_pre", "audio_mbp_disp"]:
    block = d.get(r, {})
    print(f"=== {r} keys={list(block.keys())} ===")
    for k, v in block.items():
        if k == "entries":
            print(f"  entries: {len(v)} total — last 8:")
            for e in v[-8:]:
                print(f"    {e}")
        else:
            print(f"  {k}: {v}")
    print()
