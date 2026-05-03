"""Simulate F3DEX2 DL execution for the captured hung-DL bin.
Detects infinite loops by tracking visited PCs and pushing/popping a
return-address stack on G_DL/G_ENDDL. Stops at first revisit.

The simulator only handles control-flow ops (G_DL, G_ENDDL) — it ignores
all other commands' side effects, which is what we need to find a
control-flow cycle.

Notes on RT64 semantics (rt64_gbi_f3d.cpp::runDl):
- G_DL with p0 bit16==0: push current dl as return; jump to target
- G_DL with p0 bit16==1: jump without pushing
- G_ENDDL: pop return; if stack empty, dl=null and we exit

Targets are passed through fromSegmentedMasked. For Stadium with seg 0
mapped to vaddr 0x80000000, addresses with top byte 0x00 mean seg-0
relative (so 0x00227bc8 -> vaddr 0x80227bc8). Out-of-this-DL targets
are marked but not followed (we only know this DL's bytes).
"""
import struct, sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_freeze_dl.bin"
DL_START = int(sys.argv[2], 0) if len(sys.argv) > 2 else 0x80209500
LIMIT = int(sys.argv[3]) if len(sys.argv) > 3 else 200000

with open(PATH, "rb") as f:
    data = f.read()
DL_END = DL_START + len(data)

def read_cmd(offset):
    w0 = struct.unpack(">I", data[offset:offset+4])[0]
    w1 = struct.unpack(">I", data[offset+4:offset+8])[0]
    return w0, w1

def in_dl(vaddr):
    return DL_START <= vaddr < DL_END

# Resolve a segmented or vram address.
# If top byte is 0x80, it's already vaddr.
# If top byte is 0x00, treat as seg-0 relative -> 0x80000000 + addr.
# If top byte is anything else, return as-is (we'll mark "external").
def resolve(addr):
    seg = (addr >> 24) & 0xFF
    if seg == 0:
        return 0x80000000 + (addr & 0x00FFFFFF), "seg0"
    if seg == 0x80:
        return addr, "vram"
    return addr, f"seg{seg:#x}"

pc = DL_START
stack = []
visited = {}   # offset -> step at which first seen
log = []
step = 0

while pc is not None and step < LIMIT:
    if not in_dl(pc):
        log.append(f"step={step} pc={pc:#010x}  EXTERNAL — leaving this DL; cannot continue")
        break
    off = pc - DL_START
    if off >= len(data) - 8:
        log.append(f"step={step} pc={pc:#010x}  RAN_OFF_END")
        break
    if off in visited:
        log.append(f"step={step} pc={pc:#010x} off={off:#06x}  CYCLE (first seen at step {visited[off]})")
        break
    visited[off] = step

    w0, w1 = read_cmd(off)
    op = (w0 >> 24) & 0xFF

    if op == 0xDE:   # G_DL
        branch = (w0 >> 16) & 0xFF & 1
        target_raw = w1
        target, src = resolve(target_raw)
        if branch == 0:
            # call: push return = pc + 8
            stack.append(pc + 8)
        log.append(f"step={step:5} pc={pc:#010x} off={off:#06x}  G_DL branch={branch} target={target_raw:#010x}->{target:#010x} ({src})  stack_depth={len(stack)}")
        pc = target
    elif op == 0xDF:  # G_ENDDL
        if stack:
            new_pc = stack.pop()
            log.append(f"step={step:5} pc={pc:#010x} off={off:#06x}  G_ENDDL pop -> {new_pc:#010x}  stack_depth={len(stack)}")
            pc = new_pc
        else:
            log.append(f"step={step:5} pc={pc:#010x} off={off:#06x}  G_ENDDL (stack empty) -> NULL")
            pc = None
    else:
        # Non-control-flow: advance by 8
        pc = pc + 8
    step += 1

print(f"executed {step} steps; visited {len(visited)} unique offsets")
print(f"final pc = {pc}")
print()
# Print last 50 log entries
print("=== last 50 control-flow events ===")
for e in log[-50:]:
    print(" ", e)
