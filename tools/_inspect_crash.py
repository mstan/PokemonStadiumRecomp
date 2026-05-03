"""Top-level dump summary for triaging a SEH crash."""
import json

d = json.load(open("build/last_run_report.json"))
print("reason:", d.get("reason"))
print("timestamp:", d.get("timestamp"))

seh = d.get("seh") or {}
print()
print("=== SEH ===")
for k, v in seh.items():
    if isinstance(v, (int, str, bool, float)):
        if isinstance(v, int) and v > 0xFFFF:
            print(f"  {k}: {v} ({v:#x})")
        else:
            print(f"  {k}: {v}")
    elif isinstance(v, list):
        print(f"  {k}: ({len(v)} entries)")

status = d.get("status", {})
print()
print("=== status counters ===")
for k in ("frame", "vi", "send_dl", "send_dl_gfx", "submit_gfx",
         "submit_audio", "sp_complete", "dp_complete", "update_screen",
         "external_requeues"):
    print(f"  {k:18} = {status.get(k)}")

hw = d.get("hardware", {}) or {}
print()
print("=== hardware ===")
print("  gCurrentGameState:", hw.get("gCurrentGameState"))
fl = hw.get("gFragments_loaded", [])
print(f"  gFragments_loaded: {len(fl)} entries")
for f in fl:
    if isinstance(f, dict):
        print(f"    id={f.get('id')} vaddr={f.get('vaddr'):#x} size={f.get('size'):#x}")
    else:
        print("   ", f)

print()
print("=== threads (game thread first) ===")
threads = d.get("threads", [])
# Show only the PokemonStadium / faulting thread
for t in threads:
    name = t.get("name") or "?"
    if name == "PokemonStadium" or t.get("faulting"):
        print(f"--- tid={t.get('tid')} name='{name}' faulting={t.get('faulting')} frames={len(t.get('frames', []))} ---")
        for fr in t.get("frames", []):
            print("  ", fr)
        print()
