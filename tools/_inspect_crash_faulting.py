"""Find the faulting thread."""
import json

d = json.load(open("build/last_run_report.json"))
seh = d.get("seh", {})
print("=== SEH ===")
for k, v in seh.items():
    if isinstance(v, (int, str, bool, float)):
        if isinstance(v, int) and v > 0xFFFF:
            print(f"  {k}: {v} ({v:#x})")
        else:
            print(f"  {k}: {v}")

print()
print("=== ALL threads (looking for faulting) ===")
threads = d.get("threads", [])
for t in threads:
    name = t.get("name") or "?"
    fault = t.get("faulting")
    if fault:
        print(f"\n*** FAULTING tid={t.get('tid')} name='{name}' frames={len(t.get('frames', []))} ***")
        for fr in t.get("frames", []):
            print(f"  #{fr.get('i'):02} {fr.get('name')} ({fr.get('file')}:{fr.get('line')})")

# If no thread is marked faulting, also print every PokemonStadium thread
print()
print("=== ALL PokemonStadium-named threads ===")
for t in threads:
    if (t.get("name") or "") != "PokemonStadium":
        continue
    print(f"\n--- tid={t.get('tid')} faulting={t.get('faulting')} ---")
    for fr in t.get("frames", []):
        nm = fr.get("name") or "?"
        # Only show user-relevant frames (skip kernel32/ntdll noise)
        if "PokemonStadium" in (fr.get("file") or "") or "lib\\N64ModernRuntime" in (fr.get("file") or "") or "generated" in (fr.get("file") or "") or "src\\main" in (fr.get("file") or ""):
            print(f"  #{fr.get('i'):02} {nm} ({fr.get('file')}:{fr.get('line')})")
