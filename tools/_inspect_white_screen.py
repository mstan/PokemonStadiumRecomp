"""Inspect last_run_report.json for white-screen hang state.

Surfaces:
  - submit_gfx vs dp_complete delta (confirms hang)
  - Last N interp_cf events (where RT64 is parked in the DL)
  - Final command at +offset (is it inside the captured DL?)
"""
import json, sys

REPORT = sys.argv[1] if len(sys.argv) > 1 else "F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json"
DL_BIN = sys.argv[2] if len(sys.argv) > 2 else "F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_dl_whitescreen.bin"
DL_START = 0x80209500

with open(REPORT) as f:
    r = json.load(f)
print("top-level keys:", list(r.keys()))
print()

cf = r.get("interp_cf")
if cf is None:
    print("no interp_cf section")
elif "error" in cf:
    print("interp_cf error:", cf["error"])
else:
    print("interp_cf seq:", cf.get("seq"))
    events = cf.get("events", [])
    print("interp_cf event count:", len(events))
    if events:
        print()
        print("LAST 16 interp_cf events:")
        for e in events[-16:]:
            print(" ", e)

# Check last_gfx context if present
for k in ("sp_task_recent", "ultra_recent", "libultra_recent"):
    sect = r.get(k)
    if sect:
        evs = sect.get("events") or []
        print(f"\n{k} count: {len(evs)} (last 3:)")
        for e in evs[-3:]:
            print(" ", e)
