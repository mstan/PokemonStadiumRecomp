"""Inspect last_run_report.json — focus on threads, ring tails."""
import json, sys
report = json.loads(open("F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json").read())

print("=== STATUS ===")
print(json.dumps(report.get("status",{}), indent=2))

print()
print("=== HARDWARE ===")
hw = report.get("hardware", {})
print(f"gCurrentGameState: {hw.get('gCurrentGameState')}")
print(f"D_800A7464_ptr: 0x{hw.get('D_800A7464_ptr',0):08X}")
segs = hw.get("gSegments", [])
nonzero_segs = [s for s in segs if s.get('vaddr')]
print(f"gSegments non-zero: {nonzero_segs}")
frags = hw.get("gFragments_loaded", [])
print(f"gFragments_loaded: {len(frags)} entries")
for fr in frags[:25]:
    print(f"  id={fr['id']:3} vaddr=0x{fr['vaddr']:08X} size=0x{fr['size']:X}")

print()
print("=== THREADS ===")
for t in report.get("threads", []):
    name = t.get("name") or "?"
    tid = t["tid"]
    frames = t.get("frames", [])
    print(f"\n--- tid={tid} name='{name}' frames={len(frames)} ---")
    for fr in frames:
        f = fr.get("file","?").replace("F:\\Projects\\n64recomp\\PokemonStadiumRecomp\\", "...\\")
        print(f"  #{fr['i']:02} {fr['name']} ({f}:{fr['line']})")

print()
print("=== TRACE_RECENT (last 30) ===")
tr = report.get("trace_recent", {}).get("entries", [])
for e in tr[-30:]:
    print(f"  {e}")

print()
print("=== MESG_RECENT (last 30) ===")
me = report.get("mesg_recent", {}).get("events", [])
for e in me[-30:]:
    print(f"  seq={e['seq']:7} ms={e['ms']:7} {e['op']:18} mq=0x{e['mq']:08X} msg=0x{e['msg']:08X} vb={e['vb']} va={e['va']} block={e['block']} gt={e['gt']}")
