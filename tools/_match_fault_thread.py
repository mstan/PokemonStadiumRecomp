"""Match SEH fault PC against per-thread rip to find faulting thread."""
import json

d = json.load(open("build/last_run_report.json"))
seh = d.get("seh", {})
fault_pc = seh.get("address")
print(f"SEH fault PC = {fault_pc:#018x}")
print()

threads = d.get("threads", [])
print("=== threads with rip near fault PC ===")
exact = None
for t in threads:
    rip = t.get("rip")
    if rip is None: continue
    delta = rip - fault_pc
    if abs(delta) < 0x1000:
        print(f"  tid={t.get('tid')} name='{t.get('name')}' rip={rip:#018x} delta={delta:+}")
        if delta == 0 and exact is None:
            exact = t

# Also list ALL threads with their rip / top frame for reference
print()
print("=== ALL threads ===")
for t in threads:
    rip = t.get("rip", 0) or 0
    top = (t.get("frames") or [{}])[0]
    print(f"  tid={t.get('tid'):>6} name='{t.get('name')}' rip={rip:#018x}  top={top.get('name','?')}")

# If we have a faulting thread, print its full stack
if exact:
    print()
    print(f"*** FAULTING tid={exact.get('tid')} ***")
    for fr in exact.get("frames", []):
        print(f"  #{fr.get('i'):02} {fr.get('name')}  ({fr.get('file')}:{fr.get('line')})")
