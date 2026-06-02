"""Show the audio diagnostic blocks captured in the post-mortem JSON."""
import json, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/_2026-05-10_register_pokemon_audio_uaf_pm.json"
d = json.load(open(PATH))

for key in ("status","reason","seh","interp_probe","audio_afx_in","audio_afx_out",
            "audio_mbp_pre","audio_mbp_disp","audio_mbp_chain","gdl_submit","gdl_walk"):
    v = d.get(key)
    if v is None: continue
    if isinstance(v, dict):
        print(f"=== {key} ===")
        # truncate large arrays
        for k, vv in v.items():
            if isinstance(vv, list) and len(vv) > 6:
                print(f"  {k}: list[{len(vv)}] (first 3) {vv[:3]}")
            elif isinstance(vv, str) and len(vv) > 200:
                print(f"  {k}: str[{len(vv)}] {vv[:200]}...")
            else:
                print(f"  {k}: {vv}")
        print()
    elif isinstance(v, str):
        if len(v) > 500:
            print(f"=== {key} (str[{len(v)}]) ===\n{v[:500]}...\n")
        else:
            print(f"=== {key} ===\n{v}\n")

# Specifically, show threads list briefly
if "threads" in d and isinstance(d["threads"], list):
    print(f"=== threads ({len(d['threads'])} entries) ===")
    for t in d["threads"][:8]:
        print(f"  {t}")
    if len(d["threads"]) > 8:
        print(f"  ... +{len(d['threads'])-8} more threads")
