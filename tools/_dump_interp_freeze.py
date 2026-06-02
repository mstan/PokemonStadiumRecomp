"""Dump RT64 Interpreter probe rings from the freeze post-mortem.

interp_probe = step events (per cmd processed)
interp_cf    = control-flow events (G_DL push/pop, etc.)
"""
import json
import sys

REPORT = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"
d = json.load(open(REPORT))

print("=== interp_probe keys ===")
ip = d.get("interp_probe", {})
print(f"  keys: {list(ip.keys())}")
for k, v in ip.items():
    if isinstance(v, list):
        print(f"  {k}: {len(v)} entries — last 30:")
        for e in v[-30:]:
            print(f"    {e}")
    else:
        print(f"  {k}: {v}")

print("\n=== interp_cf keys ===")
icf = d.get("interp_cf", {})
print(f"  keys: {list(icf.keys())}")
for k, v in icf.items():
    if isinstance(v, list):
        print(f"  {k}: {len(v)} entries — last 30:")
        for e in v[-30:]:
            print(f"    {e}")
    else:
        print(f"  {k}: {v}")

print("\n=== sp_task_recent (last DL submission) ===")
spt = d.get("sp_task_recent", {})
for k, v in spt.items():
    if isinstance(v, list):
        print(f"  {k}: {len(v)} entries — last 5:")
        for e in v[-5:]:
            print(f"    {e}")
    else:
        print(f"  {k}: {v}")
