"""F3DEX2 simulator backed by full RDRAM dump.
Follows G_DL chains across sub-DLs, detects cycles, prints a trace
that ends at the first cycle (or when execution leaves RDRAM).

The hang happens because RT64::Interpreter::processDisplayLists never
returns — i.e., dl never becomes null. So this simulator should hit
a cycle in the same place RT64 is stuck.
"""
import struct, sys

RDRAM_PATH = "build/last_run_rdram.bin"
TASK_DATA_PTR = int(sys.argv[1], 0) if len(sys.argv) > 1 else 0x80209500
LIMIT = int(sys.argv[2]) if len(sys.argv) > 2 else 5_000_000

with open(RDRAM_PATH, "rb") as f:
    rdram = f.read()
RDRAM_SIZE = len(rdram)
assert RDRAM_SIZE == 0x800000, f"unexpected rdram size {RDRAM_SIZE:#x}"

def read_cmd(vaddr):
    seg = (vaddr >> 24) & 0xFF
    if seg == 0:
        offset = vaddr & 0x00FFFFFF
    elif seg == 0x80:
        offset = vaddr & 0x00FFFFFF
    else:
        return None
    if offset >= RDRAM_SIZE - 8:
        return None
    w0 = struct.unpack(">I", rdram[offset:offset+4])[0]
    w1 = struct.unpack(">I", rdram[offset+4:offset+8])[0]
    return w0, w1

OP_NAME = {
    0xDE: "G_DL", 0xDF: "G_ENDDL", 0xDD: "G_LOAD_UCODE",
    0x04: "G_BRANCH_Z", 0xE9: "G_RDPFULLSYNC",
}

pc = TASK_DATA_PTR
stack = []
visited = {}     # vaddr -> visit count
log = []
step = 0
cycle_step = -1
exit_reason = "completed"
# RSP segment table — F3DEX2 uses bottom 4 bits of seg-addr top byte
segments = [0] * 16

while pc is not None and step < LIMIT:
    cmd = read_cmd(pc)
    if cmd is None:
        exit_reason = f"pc={pc:#010x} OUT_OF_RDRAM"
        break
    # No cycle detection — run forward to LIMIT or natural termination.
    visited[pc] = visited.get(pc, 0) + 1

    w0, w1 = cmd
    op = (w0 >> 24) & 0xFF

    if op == 0xDE:  # G_DL
        branch = (w0 >> 16) & 0x01
        target = w1
        # F3DEX2 branch: bit16 == 1 means no-push (jump), 0 means push (call)
        if branch == 0:
            stack.append(pc + 8)
        # Resolve via RT64's fromSegmented:
        #   segments[(target>>24)&0x0F] + (target & 0x00FFFFFF)
        seg_id = (target >> 24) & 0x0F
        target_v = (segments[seg_id] + (target & 0x00FFFFFF)) & 0xFFFFFFFF
        log.append((step, pc, w0, w1, op, branch, target, len(stack), target_v))
        pc = target_v
    elif op == 0xDF:  # G_ENDDL
        if stack:
            new_pc = stack.pop()
            log.append((step, pc, w0, w1, op, 0, new_pc, len(stack), new_pc))
            pc = new_pc
        else:
            log.append((step, pc, w0, w1, op, 0, 0, 0, 0))
            pc = None
            exit_reason = "G_ENDDL with empty stack -> dl=null (clean exit)"
            break
    elif op == 0xDB:  # G_MOVEWORD — track segment register writes
        # F3DEX2 G_MOVEWORD layout:
        #   w0 = (0xDB << 24) | (index << 16) | offset
        #   w1 = data
        # G_MW_SEGMENT index = 0x06; offset = (segment_id << 2)
        index = (w0 >> 16) & 0xFF
        offset = w0 & 0xFFFF
        if index == 0x06:
            seg_id = (offset >> 2) & 0x0F
            segments[seg_id] = w1 & 0x00FFFFFF
        pc = pc + 8
    else:
        # Non-control-flow: just advance
        pc = pc + 8
    step += 1

print(f"executed {step} steps; visited {len(visited)} unique PCs")
print(f"exit: {exit_reason}")
print(f"final stack depth: {len(stack)}")
print()
# Show top-visited PCs (potential infinite-loop participants)
top = sorted(visited.items(), key=lambda x: -x[1])[:15]
print("=== top visited PCs (visit count) ===")
for pc_v, ct in top:
    print(f"  pc={pc_v:#010x}  visits={ct}")
print()
print(f"=== final segments[] ===")
for i, seg in enumerate(segments):
    if seg != 0:
        print(f"  segments[{i}] = {seg:#010x}")
print()

# Always show last 30 control-flow events
print()
print("=== last 30 control-flow events ===")
for e in log[-30:]:
    s = e[0]; p = e[1]; w0 = e[2]; w1 = e[3]; op = e[4]
    n = OP_NAME.get(op, f"OP_{op:02X}")
    if op == 0xDE:
        br = e[5]; tgt = e[6]; sd = e[7]; tgt_v = e[8]
        print(f"  step={s:7} pc={p:#010x}  G_DL br={br} target={tgt:#010x} ({tgt_v:#010x})  stack={sd}")
    elif op == 0xDF:
        tgt = e[6]; sd = e[7]
        print(f"  step={s:7} pc={p:#010x}  G_ENDDL pop->{tgt:#010x}  stack={sd}")
    else:
        print(f"  step={s:7} pc={p:#010x}  {n}")
