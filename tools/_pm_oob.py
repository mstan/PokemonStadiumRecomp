"""Show the raw w0/w1/target_or_pop for the OOB cf event(s) leading up to runaway."""
import json

p = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json'
d = json.load(open(p))
ip = d['interp_probe']
base = ip['rdram_host_base']

cf = d['interp_cf']
events = cf['events']
print(f'rdram_host_base = 0x{base:016X}')


def to_vaddr(pc):
    diff = pc - base
    return (0x80000000 | (diff & 0x7FFFFF)) if 0 <= diff < 0x800000 else None


# Last 15 cf events with FULL raw values
print('\nLast 15 cf events (raw):')
for e in events[-15:]:
    pc = e['pc']
    pc_v = to_vaddr(pc)
    tgt = e['target_or_pop']
    tgt_v = to_vaddr(tgt)
    print(f"  seq~?  step={e['step']:>5} {e['op']:<8} "
          f"pc=0x{pc:016X} ({'%08X'%pc_v if pc_v else 'OOB'}) "
          f"w0=0x{e['w0']:08X} w1=0x{e['w1']:08X} "
          f"target_or_pop=0x{tgt:016X} ({'%08X'%tgt_v if tgt_v else 'OOB'}) "
          f"depth={e['depth']}")

# Find all events with OOB pc or target_or_pop
print('\nAll OOB events:')
for i, e in enumerate(events):
    pc = e['pc']
    tgt = e['target_or_pop']
    pc_v = to_vaddr(pc)
    tgt_v = to_vaddr(tgt)
    if pc_v is None or (tgt_v is None and e['op'] == 'G_DL'):
        print(f'  idx={i} step={e["step"]} {e["op"]:<8} pc=0x{pc:016X} (vaddr={pc_v}) tgt=0x{tgt:016X} (vaddr={tgt_v}) w0=0x{e["w0"]:08X} w1=0x{e["w1"]:08X}')

# Step in current task starts at 0. If max step is small but interp_probe step is huge,
# the cf events are from an OLD task. Verify task continuity:
print(f'\nCurrent interp_probe step = {ip["step"]}')
print(f'Max step in cf events: {max(e["step"] for e in events)}')
print(f'Min step in cf events: {min(e["step"] for e in events)}')
