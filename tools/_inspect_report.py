"""Inspect last_run_report.json sp_task ring + status counters."""
import json, sys

p = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"
with open(p) as f:
    d = json.load(f)

st = d.get("status", {})
print("=== status counters ===")
for k in ("frame", "send_dl", "send_dl_gfx", "submit_gfx",
         "submit_audio", "sp_complete", "dp_complete", "update_screen"):
    print(f"  {k:18} = {st.get(k)}")

sp = d.get("sp_task_recent", {})
events = sp.get("events", [])
gfx = [e for e in events if e.get("type") == "M_GFXTASK"]
print(f"\nsp_task_recent: write_idx={sp.get('write_idx')}  events={len(events)}  gfx={len(gfx)}")
if gfx:
    last = gfx[-1]
    print(f"  last gfx: seq={last['seq']} frame={last['frame']} send_dl={last['send_dl']}")
    print(f"            data_ptr={last['data_ptr']:#010x} data_size={last['data_size']} ucode={last['ucode']:#010x}")
else:
    print("  NO M_GFXTASK in window")
