"""Inspect last_run_report.json for attract-stall diagnostics."""
import json

r = json.load(open('build/last_run_report.json'))

ip = r.get('interp_probe', {})
host_base = ip.get('rdram_host_base', 0)
dl_start = ip.get('dl_start', 0)
cur_pc = ip.get('current_pc', 0)
step = ip.get('step', 0)
task_index = ip.get('task_index', 0)
print(f"=== INTERP PROBE ===")
print(f"  task_index = {task_index}")
print(f"  step       = {step:,}")
print(f"  dl_start   = 0x{dl_start:08X} (= rdram phys 0x{dl_start:X}, N64 vaddr 0x{0x80000000 + dl_start:08X})")
print(f"  current_pc = host 0x{cur_pc:X}")
print(f"  rdram_host = host 0x{host_base:X}")
print(f"  cur_pc - rdram_base = 0x{cur_pc - host_base:X} (rdram offset)")
print(f"  -> walking N64 vaddr 0x{0x80000000 + (cur_pc - host_base):08X}")
print()

print(f"=== RECENT_PCS WINDOW (rdram offsets relative to host_base) ===")
pcs = ip.get('recent_pcs', [])
offs = [p - host_base for p in pcs]
print(f"  count={len(pcs)}, min=0x{min(offs):X}, max=0x{max(offs):X}, span={max(offs)-min(offs):,}")
print(f"  unique offsets: {len(set(offs))}")
# bucket per 0x100 to see clustering
buckets = {}
for o in offs:
    b = o // 0x100
    buckets[b] = buckets.get(b, 0) + 1
print(f"  buckets (per 0x100):")
for b in sorted(buckets):
    print(f"    0x{b*0x100:X}..0x{(b+1)*0x100-1:X}  ({buckets[b]} hits)  vaddr 0x{0x80000000 + b*0x100:08X}")
print()

if 'gdl_submit' in r:
    print('=== GDL_SUBMIT (last 8) ===')
    g = r['gdl_submit']
    events = g.get('events') if isinstance(g, dict) else g
    if events:
        for e in events[-8:]:
            print(' ', e)
    print()

if 'gdl_walk' in r:
    print('=== GDL_WALK (last 16) ===')
    g = r['gdl_walk']
    events = g.get('events') if isinstance(g, dict) else g
    if events:
        for e in events[-16:]:
            print(' ', e)
    print()

if 'interp_cf' in r:
    print('=== INTERP_CF (last 16 control-flow events) ===')
    cf = r['interp_cf']
    events = cf.get('events') if isinstance(cf, dict) else cf
    if events:
        for ev in events[-16:]:
            print(' ', ev)
    print()
