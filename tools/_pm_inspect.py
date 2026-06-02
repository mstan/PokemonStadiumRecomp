"""Inspect interp_probe section of build/last_run_report.json after a softlock dump."""
import json, sys

p = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json'
d = json.load(open(p))
ip = d.get('interp_probe', {})
print('=== RT64 interp_probe (post-mortem state at softlock) ===')
print(f'  seq           = {ip.get("seq")}')
print(f'  step          = {ip.get("step")}')
print(f'  task_index    = {ip.get("task_index")}')
print(f'  current_pc    = 0x{ip.get("current_pc", 0):016X}')
print(f'  dl_start      = 0x{ip.get("dl_start", 0):08X}')
print(f'  rdram_host_base = 0x{ip.get("rdram_host_base", 0):016X}')
rpcs = ip.get('recent_pcs', [])
print(f'  recent_pcs count: {len(rpcs)}')

base = ip.get('rdram_host_base', 0)


def to_vaddr(pc):
    diff = pc - base
    return (0x80000000 | (diff & 0x7FFFFF)) if 0 <= diff < 0x800000 else None


print('\nlast 40 recent_pcs (as vaddrs):')
for pc in rpcs[-40:]:
    v = to_vaddr(pc)
    print(f'  pc=0x{pc:016X}  -> vaddr=0x{v:08X}' if v else f'  pc=0x{pc:016X}  (out of rdram)')

# Distinct PCs in the last 100
import collections
last100 = rpcs[-100:]
freq = collections.Counter(last100)
print(f'\nfrequency in last {len(last100)} PCs:')
for pc, cnt in freq.most_common(8):
    v = to_vaddr(pc)
    print(f'  {cnt:>4}× pc=0x{pc:016X}  vaddr=0x{v:08X}' if v else f'  {cnt:>4}× pc=0x{pc:016X}')

cf = d.get('interp_cf', {})
print(f'\n=== interp_cf (control-flow events) seq={cf.get("seq")} ===')
events = cf.get('events', [])
print(f'event count: {len(events)}')
print('\nlast 20 cf events:')
for e in events[-20:]:
    pc_v = to_vaddr(e['pc'])
    tgt_v = to_vaddr(e['target_or_pop']) if e['op'] == 'G_DL' else None
    print(f"  step={e['step']:>5} pc=0x{e['pc']:016X} ({pc_v and ('0x%08X'%pc_v) or '?'}) "
          f"{e['op']:<8} w1=0x{e['w1']:08X} target_v=0x{tgt_v:08X} depth={e['depth']}"
          if tgt_v else
          f"  step={e['step']:>5} pc=0x{e['pc']:016X} ({pc_v and ('0x%08X'%pc_v) or '?'}) "
          f"{e['op']:<8} w1=0x{e['w1']:08X} pop=0x{e['target_or_pop']:016X} depth={e['depth']}")
