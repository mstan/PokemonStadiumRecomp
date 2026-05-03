"""Inspect the RT64 interpreter probe ring captured in last_run_report.json."""
import json
from collections import Counter

d = json.load(open("build/last_run_report.json"))
ip = d.get("interp_probe", {})

print("=== interp_probe ===")
print(f"  seq        = {ip.get('seq')}")
print(f"  step       = {ip.get('step')}")
print(f"  task_index = {ip.get('task_index')}")
cpc = ip.get("current_pc")
ds  = ip.get("dl_start")
rb  = ip.get("rdram_host_base", 0)
print(f"  current_pc = {cpc:#018x}" if cpc else "  current_pc = 0")
print(f"  dl_start   = {ds:#018x}" if ds else "  dl_start = 0")
print(f"  rdram_host = {rb:#018x}" if rb else "  rdram_host = 0")
recent = ip.get("recent_pcs", [])
print(f"  recent_pcs ({len(recent)} entries)")

# Translate host PCs to RDRAM offsets / vaddrs where possible
def translate(host_pc):
    if rb == 0:
        return None
    off = host_pc - rb
    if 0 <= off < 0x800000:
        return off, 0x80000000 + off
    return None

print()
if cpc:
    t = translate(cpc)
    if t:
        print(f"  current_pc translates to RDRAM offset {t[0]:#08x} = vaddr {t[1]:#010x}")
    else:
        print(f"  current_pc is OUT OF RDRAM (host - rdram_base = {cpc - rb:#x})")
print()

c = Counter(recent)
top = c.most_common(15)
print("=== top hot PCs in recent window (host pointers) ===")
for pc, ct in top:
    t = translate(pc)
    suffix = f"  (vaddr {t[1]:#010x})" if t else "  (OOB)"
    print(f"  pc={pc:#018x}  visits={ct}{suffix}")
print()

# Find runs where the same PC repeats consecutively or in tight cycles
print("=== last 30 PCs (chronological) ===")
for pc in recent[-30:]:
    t = translate(pc)
    suffix = f"  (vaddr {t[1]:#010x})" if t else "  (OOB)"
    print(f"  pc={pc:#018x}{suffix}")
