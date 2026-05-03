"""Inspect the RT64 interpreter CF (control-flow) ring captured in
last_run_report.json. Locates the LAST G_DL or G_ENDDL event before
the OOB walkaway, which is the proximate cause of the runaway."""
import json

d = json.load(open("build/last_run_report.json"))
ip = d.get("interp_probe", {})
cf = d.get("interp_cf", {})
rb = ip.get("rdram_host_base", 0)
cur_pc = ip.get("current_pc", 0)
cur_step = ip.get("step", 0)
events = cf.get("events", [])
print(f"interp_probe step={cur_step}  current_pc={cur_pc:#018x}  rdram_host_base={rb:#018x}")
print(f"interp_cf seq={cf.get('seq')}  events={len(events)}")
print()

def to_v(host_pc):
    if rb == 0:
        return None
    off = host_pc - rb
    if 0 <= off < 0x800000:
        return 0x80000000 + off
    return None

print("=== last 25 control-flow events ===")
for e in events[-25:]:
    pc = e["pc"]
    tgt = e["target_or_pop"]
    pc_v = to_v(pc)
    tgt_v = to_v(tgt)
    pcs = f"{pc:#x}" + (f" ({pc_v:#010x})" if pc_v else " (OOB)")
    tgts = f"{tgt:#x}" + (f" ({tgt_v:#010x})" if tgt_v else " (OOB)")
    op = e["op"]
    w1 = e["w1"]
    branch_or = ""
    if op == "G_DL":
        branch_or = f"  br={(e['w0']>>16)&1}  raw_target={w1:#010x}"
    print(f"  step={e['step']:>9}  pc={pcs:32s}  {op:8s}  -> {tgts}{branch_or}")
print()

# Find the last event whose target/pop is in low RDRAM (boot region 0x80000000..0x80010000)
print("=== events whose target/pop is in BOOT REGION (vaddr 0x80000000..0x80010000) ===")
for e in events:
    tgt_v = to_v(e["target_or_pop"])
    if tgt_v and 0x80000000 <= tgt_v < 0x80010000:
        print(f"  step={e['step']}  pc={e['pc']:#x}  {e['op']}  raw_target={e['w1']:#010x}  -> {e['target_or_pop']:#x} (vaddr {tgt_v:#010x})")
