"""Search cf events for any with PC or target in 0x80002xxx range —
that's where RT64 is currently stuck walking."""
import json, collections

p = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/last_run_report.json'
d = json.load(open(p))
ip = d['interp_probe']
base = ip['rdram_host_base']


def to_vaddr(pc):
    diff = pc - base
    return (0x80000000 | (diff & 0x7FFFFF)) if 0 <= diff < 0x800000 else None


cf = d.get('interp_cf', {})
events = cf.get('events', [])
print(f'total cf events: {len(events)}, seq base = {cf.get("seq", 0) - len(events)}')

# All events in chronological order. Look for any with target or pc in 0x80002xxx range.
hits_low = []
for e in events:
    pc_v = to_vaddr(e['pc'])
    tgt_v = to_vaddr(e['target_or_pop']) if e['op'] == 'G_DL' else None
    if pc_v and 0x80002000 <= pc_v < 0x80003000:
        hits_low.append(('pc-low', e, pc_v, tgt_v))
    if tgt_v and 0x80002000 <= tgt_v < 0x80003000:
        hits_low.append(('target-low', e, pc_v, tgt_v))

print(f'\ncf events with pc OR target in 0x80002xxx: {len(hits_low)}')
for kind, e, pc_v, tgt_v in hits_low[:20]:
    print(f'  {kind:<12} step={e["step"]:>5} {e["op"]:<8} pc_v={pc_v and ("0x%08X"%pc_v) or "?"} '
          f'target_v={tgt_v and ("0x%08X"%tgt_v) or "?"} w1=0x{e["w1"]:08X} depth={e["depth"]}')

# Also: the very LAST event in chronological order
print('\nLast 5 cf events (chronological):')
for e in events[-5:]:
    pc_v = to_vaddr(e['pc'])
    tgt_v = to_vaddr(e['target_or_pop']) if e['op'] == 'G_DL' else None
    print(f'  step={e["step"]:>5} {e["op"]:<8} pc=0x{pc_v:08X}' if pc_v else f'  step={e["step"]:>5} {e["op"]:<8} pc=?',
          end='')
    if e['op'] == 'G_DL':
        print(f' target=0x{tgt_v:08X}' if tgt_v else ' target=?', end='')
        print(f' push=({"calc from w0 needed"})')
    else:
        print(f' pop_v=0x{tgt_v:08X}' if tgt_v else ' pop=?')

# Distribution of which DLs were entered — high 12 bits of target_v for G_DL events
print('\nG_DL target vaddr buckets (high 12 bits):')
buckets = collections.Counter()
for e in events:
    if e['op'] != 'G_DL':
        continue
    tgt_v = to_vaddr(e['target_or_pop'])
    if tgt_v is None:
        buckets['oob'] += 1
        continue
    buckets[(tgt_v >> 20) & 0xFFF] += 1
for k, v in sorted(buckets.items(), key=lambda kv: -kv[1])[:15]:
    label = f'0x{k:03X}xxxxx' if isinstance(k, int) else str(k)
    print(f'  {label}: {v}')
