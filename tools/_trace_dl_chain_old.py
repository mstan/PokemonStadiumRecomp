"""Walk the full interp_cf event ring backward from the LAST event to
trace the DL chain that led the interpreter into the bad region.

interp_cf events have:
  step, pc (host), w0, w1, target_or_pop (host addr resolved), op, depth

We want to see the sequence of G_DL hops: each event's target_or_pop
becomes the next event's pc. Build the chain and identify which
top-level DL CALL/BRANCH target started it.
"""
import json
import sys

REPORT = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"
d = json.load(open(REPORT))

icf = d.get("interp_cf", {})
events = icf.get("events", [])
seq = icf.get("seq", 0)
print(f"interp_cf: total seq={seq}, ring entries={len(events)}")

ip = d.get("interp_probe", {})
rdram_base = ip.get("rdram_host_base", 0)
print(f"rdram_host_base = {rdram_base} = 0x{rdram_base:X}")

def to_off(host):
    if host >= rdram_base and host < rdram_base + 0x800000:
        return host - rdram_base
    return None

print(f"\n=== first 5 events (oldest in ring) ===")
for e in events[:5]:
    pc_off = to_off(e['pc'])
    tgt_off = to_off(e['target_or_pop'])
    pc_str = f"vaddr=0x{0x80000000+pc_off:08X}" if pc_off is not None else f"out_of_rdram pc={e['pc']}"
    tgt_str = f"vaddr=0x{0x80000000+tgt_off:08X}" if tgt_off is not None else f"out_of_rdram tgt={e['target_or_pop']}"
    print(f"  step={e['step']} {e['op']} depth={e['depth']}  pc:{pc_str}  ->  tgt:{tgt_str}  w0=0x{e['w0']:08X} w1=0x{e['w1']:08X}")

print(f"\n=== last 30 events (most recent — chain that led to freeze) ===")
for e in events[-30:]:
    pc_off = to_off(e['pc'])
    tgt_off = to_off(e['target_or_pop'])
    pc_str = f"v={0x80000000+pc_off:08X}" if pc_off is not None else f"OOR pc=0x{e['pc']:X}"
    tgt_str = f"v={0x80000000+tgt_off:08X}" if tgt_off is not None else f"OOR tgt=0x{e['target_or_pop']:X}"
    print(f"  step={e['step']:5d} {e['op']:10s} d={e['depth']}  pc:{pc_str}  ->  tgt:{tgt_str}  w0=0x{e['w0']:08X} w1=0x{e['w1']:08X}")

# Bucket events by pc-region to spot the transition into the bad region.
print(f"\n=== unique pc regions visited (count, first/last step) ===")
from collections import defaultdict
buckets = defaultdict(lambda: {"count": 0, "first": None, "last": None})
for e in events:
    pc_off = to_off(e['pc'])
    if pc_off is None:
        bucket = "OOR"
    else:
        bucket = f"0x802{(pc_off >> 12) & 0xFFFFF:05X}xxx"
    b = buckets[bucket]
    b["count"] += 1
    if b["first"] is None: b["first"] = e["step"]
    b["last"] = e["step"]
for k in sorted(buckets):
    v = buckets[k]
    print(f"  {k}: {v['count']:5d} events  steps {v['first']}..{v['last']}")
