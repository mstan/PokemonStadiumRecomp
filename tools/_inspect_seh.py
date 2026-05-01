"""Show SEH details + all PokemonStadium thread stacks."""
import json
d = json.load(open("F:/Projects/PokemonStadiumRecomp/build/last_run_report.json"))
print("reason:", d.get("reason"))
print("seh:", json.dumps(d.get("seh"), indent=2))
print()
print("status:", json.dumps(d.get("status"), indent=2))
print()
hw = d.get("hardware", {})
print(f"gCurrentGameState: {hw.get('gCurrentGameState')}")
print()
print("=== PokemonStadium threads ===")
for t in d.get("threads", []):
    if t.get("name") == "PokemonStadium" and len(t.get("frames",[])) > 6:
        print(f"tid={t['tid']} frames={len(t['frames'])}")
        for fr in t.get("frames", [])[:20]:
            f = fr.get("file","?").replace("\\", "/")
            short = f.split("PokemonStadiumRecomp/")[-1]
            print(f"  #{fr['i']:02} {fr['name']} ({short}:{fr['line']})")
        print()

# Find non-PokemonStadium threads with deep stacks (crash candidates)
print("=== other deep stacks (potential crash) ===")
for t in d.get("threads", []):
    nm = t.get("name") or "?"
    if nm == "PokemonStadium": continue
    if len(t.get("frames",[])) > 5:
        print(f"tid={t['tid']} name='{nm}' frames={len(t['frames'])}")
        for fr in t.get("frames", [])[:10]:
            f = fr.get("file","?").replace("\\", "/")
            short = f.split("PokemonStadiumRecomp/")[-1]
            print(f"  #{fr['i']:02} {fr['name']} ({short}:{fr['line']})")
        print()
