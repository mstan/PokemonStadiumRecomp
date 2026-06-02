"""Inspect the post-mortem JSON; surface keys, errlog tail, and any
voice-related state."""
import json, sys, re

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/_2026-05-10_register_pokemon_audio_uaf_pm.json"
d = json.load(open(PATH))

print("Top-level keys:")
for k in d.keys():
    v = d[k]
    desc = type(v).__name__
    if hasattr(v, "__len__"):
        desc += f" len={len(v)}"
    print(f"  {k:30s} {desc}")

print()
print("=== errlog (full) ===")
errlog = d.get("errlog", "") or d.get("errLog", "") or ""
print(errlog[-4000:])
print()

# Search for voice_check lines
print("=== '[voice_check]' lines anywhere in dump ===")
flat = json.dumps(d)
for m in re.findall(r"\[voice_check\][^\\\"]+", flat):
    print(m)
print()

print("=== first 60 lines of any 'audio' / 'voice' diagnostic field ===")
for k, v in d.items():
    if isinstance(v, str) and ("voice" in v.lower() or "n_alLoadParam" in v or "BAD v1" in v):
        print(f"--- {k} ---")
        print(v[:2000])
