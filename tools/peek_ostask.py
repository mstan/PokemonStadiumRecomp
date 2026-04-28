"""peek_ostask.py - dump Ares' aspMain OSTask + audio buffer state.

Why this exists:
    The L_11E4 dispatch-loop hang in Stadium's recompiled aspMain may
    be caused by Stadium feeding aspMain a different OSTask than what
    real RSP / Ares processes. The handoff observed:

      * Stadium hook log: data_ptr=0x800C8810 data_size=0x4C8 chunk=0x140
      * Ares' first L_10EC trace event: dma_dram_addr=0x800CAA10
        (= Stadium's data_ptr + 0x2200 = 17 chunks of 0x140)

    Either Ares is already 17 chunks deep when the boot trace starts,
    OR Ares' OSTask actually points at a different RDRAM region.

    rspboot DMAs the OSTask header into RSP DMEM at offset 0xFC0; that
    copy persists while aspMain runs. Reading DMEM[0xFC0..0xFFF] tells
    us EXACTLY what OSTask Ares' aspMain is processing.

How it works:
    1. Spawn ares_oracle_server (or use one passed in).
    2. step_frame n=N until aspMain has fired (default 60).
    3. read_memory at vaddr 0xA4000FC0 for 0x40 bytes -> OSTask.
    4. Parse all 16 OSTask_t fields.
    5. read_memory at OSTask.data_ptr for the first 0x80 bytes
       (so we can eyeball the audio command list).
    6. Pull Ares' boot snapshot for the first ~10 L_10EC entries and
       print their dma_mem_addr / dma_dram_addr / r28 / r29 / r31 to
       compare against Stadium's hung-state register snapshot.
    7. Print a verdict: if OSTask matches Stadium's hook log, the
       hang is NOT input-state divergence; if it differs, we have
       our root cause.

Usage:
    python tools/peek_ostask.py [--ares-port 4372] [--rom path]
                                [--ares-frames 60] [--oracle-server path]
                                [--keep-server]
"""
from __future__ import annotations

import argparse
import json
import socket
import struct
import subprocess
import sys
import time
from contextlib import contextmanager
from typing import Optional


# Stadium's observed hook-log values (from session handoff).
STADIUM_HOOK = {
    "data_ptr":  0x800C8810,
    "data_size": 0x4C8,
    "chunk":     0x140,
}

# RSP DMEM is at physical 0x04000000 (4KiB). KSEG1 alias = 0xA4000000.
# rspboot copies the OSTask to DMEM[0xFC0..0xFFF].
DMEM_OSTASK_VADDR = 0xA4000FC0
OSTASK_SIZE       = 0x40

# OSTask_t field layout (matches libultra PR/sptask.h).
OSTASK_FIELDS = [
    ("type",             0x00, 4),
    ("flags",            0x04, 4),
    ("ucode_boot",       0x08, 4),
    ("ucode_boot_size",  0x0C, 4),
    ("ucode",            0x10, 4),
    ("ucode_size",       0x14, 4),
    ("ucode_data",       0x18, 4),
    ("ucode_data_size",  0x1C, 4),
    ("dram_stack",       0x20, 4),
    ("dram_stack_size",  0x24, 4),
    ("output_buff",      0x28, 4),
    ("output_buff_size", 0x2C, 4),
    ("data_ptr",         0x30, 4),
    ("data_size",        0x34, 4),
    ("yield_data_ptr",   0x38, 4),
    ("yield_data_size",  0x3C, 4),
]


def _send(sock: socket.socket, f, payload):
    if isinstance(payload, str):
        payload = {"cmd": payload}
    sock.sendall((json.dumps(payload) + "\n").encode())
    return json.loads(f.readline())


@contextmanager
def tcp_client(host: str, port: int, attempts: int = 30):
    sock = None
    for _ in range(attempts):
        try:
            sock = socket.create_connection((host, port), timeout=5)
            break
        except (ConnectionRefusedError, OSError):
            time.sleep(0.2)
    if sock is None:
        raise RuntimeError(f"Could not connect to {host}:{port}")
    try:
        yield sock, sock.makefile("rb")
    finally:
        sock.close()


def hex_to_bytes(s: str) -> bytes:
    return bytes.fromhex(s)


def read_memory(sock, f, addr: int, length: int) -> bytes:
    r = _send(sock, f, {"cmd": "read_memory", "addr": addr, "len": length})
    if not r.get("ok"):
        raise RuntimeError(f"read_memory({addr:#x}, {length}) failed: {r}")
    return hex_to_bytes(r["bytes"])


def parse_ostask(buf: bytes) -> dict:
    out = {}
    for name, off, sz in OSTASK_FIELDS:
        out[name] = struct.unpack(">I", buf[off:off + sz])[0]
    return out


def fmt_addr(a: int) -> str:
    if a == 0:
        return "NULL"
    return f"0x{a:08X}"


def print_ostask(label: str, t: dict) -> None:
    print(f"  {label}:")
    for name, _, _ in OSTASK_FIELDS:
        v = t[name]
        # size fields are decimal-friendly; addresses are hex.
        if name.endswith("_size") or name == "type" or name == "flags":
            print(f"    {name:18s} = {v:#010x} ({v})")
        else:
            print(f"    {name:18s} = {fmt_addr(v)}")


def fetch_boot_trace(sock, f, total: int, batch: int = 1024) -> list[dict]:
    """Page through the boot snapshot (server caps each request at
    1024 events, but boot is 32K)."""
    out: list[dict] = []
    pos = 0
    while pos < total:
        n = min(batch, total - pos)
        r = _send(sock, f, {"cmd": "rsp_trace_boot",
                            "start": pos, "n": n})
        evs = r.get("events", [])
        if not evs:
            break
        out.extend(evs)
        pos += len(evs)
    return out


def find_label_entries(events: list[dict], target_pc: int,
                       want: int) -> list[dict]:
    """Filter events to ones that LANDED at target_pc as a branch
    target (i.e., the previous event's PC + 4 != target_pc, OR it's
    the first event)."""
    out = []
    prev_pc = None
    for ev in events:
        pc = int(ev["pc"]) & 0xFFF
        if pc == target_pc and (prev_pc is None or
                                ((prev_pc + 4) & 0xFFF) != target_pc):
            out.append(ev)
            if len(out) >= want:
                break
        prev_pc = pc
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ares-port", type=int, default=4372)
    ap.add_argument("--rom", default="F:/Projects/PokemonStadiumRecomp/baserom.z64")
    ap.add_argument("--ares-frames", type=int, default=60)
    ap.add_argument("--oracle-server",
                    default="F:/Projects/N64Recomp/build-aresbridge/"
                            "ares-bridge/Release/ares_oracle_server.exe",
                    help="Path to ares_oracle_server.exe. Pass empty to "
                         "skip spawning (assume already running).")
    ap.add_argument("--keep-server", action="store_true",
                    help="Don't terminate the spawned oracle on exit")
    args = ap.parse_args(argv)

    proc: Optional[subprocess.Popen] = None
    if args.oracle_server:
        # Kill any existing instance first; the server insists on
        # owning the port.
        subprocess.run(["taskkill", "/IM", "ares_oracle_server.exe", "/F"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.5)
        print(f"[1/4] Spawning oracle: {args.oracle_server}")
        proc = subprocess.Popen(
            [args.oracle_server, args.rom, str(args.ares_port)],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )

    try:
        with tcp_client("127.0.0.1", args.ares_port) as (s, f):
            s.settimeout(120)
            print(f"[2/4] Stepping Ares forward {args.ares_frames} frames...")
            r = _send(s, f, {"cmd": "step_frame", "n": args.ares_frames})
            if not r.get("ok"):
                print(f"  step_frame failed: {r}")
                return 3
            print(f"  done ({r['frames']} frames, "
                  f"trace_count={r['trace_count']})")

            print(f"[3/4] Reading RSP DMEM[0xFC0..0xFFF] (OSTask copy)...")
            ostask_buf = read_memory(s, f, DMEM_OSTASK_VADDR, OSTASK_SIZE)
            t = parse_ostask(ostask_buf)
            print_ostask("Ares' OSTask in DMEM", t)

            # Read the first 0x80 bytes of data_ptr so we can confirm
            # the audio command list looks plausible.
            data_ptr = t["data_ptr"]
            if data_ptr and data_ptr >= 0x80000000:
                print(f"\n  Reading first 0x40 bytes at OSTask.data_ptr "
                      f"({fmt_addr(data_ptr)}):")
                head = read_memory(s, f, data_ptr, 0x40)
                hex_pairs = " ".join(
                    f"{head[i]:02x}{head[i+1]:02x}"
                    f"{head[i+2]:02x}{head[i+3]:02x}"
                    for i in range(0, len(head), 4))
                print(f"    {hex_pairs}")
            else:
                print(f"  (data_ptr={fmt_addr(data_ptr)} is not in K0/K1 "
                      "RDRAM; skipping data dump)")

            # Need to know boot ring size first.
            st = _send(s, f, "status")
            boot_total = int(st.get("trace_boot_count", 0))
            print(f"\n[4/4] Scanning {boot_total} boot-trace events for "
                  "L_10EC entries...")
            boot = fetch_boot_trace(s, f, boot_total)
            l10ec = find_label_entries(boot, target_pc=0x10EC, want=10)
            l11e4 = find_label_entries(boot, target_pc=0x11E4, want=10)
            l1080 = find_label_entries(boot, target_pc=0x1080, want=10)
            def dump(label, entries):
                if not entries:
                    print(f"  (no {label} entries in boot snapshot)")
                    return
                print(f"  {label} ({len(entries)} entries):")
                print("    seq      dma_mem  dma_dram        r28          r29     r31")
                for ev in entries:
                    print(f"    {ev['seq']:7d}  "
                          f"0x{int(ev['dma_mem_addr']):04x}    "
                          f"0x{int(ev['dma_dram_addr']):08x}  "
                          f"0x{int(ev['gpr'][28]):08x}  "
                          f"0x{int(ev['gpr'][29]):08x} "
                          f"0x{int(ev['gpr'][31]):04x}")
            dump("L_1080 (aspMain entry?)", l1080)
            dump("L_10EC", l10ec)
            dump("L_11E4", l11e4)

            # Find what the FIRST aspMain-context OSTask was: when r28
            # in early aspMain runs, it points at the OSTask's data
            # processing region. The very first event is often rspboot;
            # the first event with PC < 0x1000 is rspboot epilogue, then
            # PC jumps to aspMain's text base. Print first 8 events and
            # first 8 events at PC>=0x1000 to see the boot transition.
            print()
            print("  First 8 boot events (rspboot context):")
            print("    seq    pc      dma_mem  dma_dram      r28        r29     r31")
            for ev in boot[:8]:
                print(f"    {ev['seq']:6d}  "
                      f"0x{int(ev['pc']):04x}  "
                      f"0x{int(ev['dma_mem_addr']):04x}    "
                      f"0x{int(ev['dma_dram_addr']):08x}  "
                      f"0x{int(ev['gpr'][28]):08x} "
                      f"0x{int(ev['gpr'][29]):08x} "
                      f"0x{int(ev['gpr'][31]):04x}")

            # First event with PC in aspMain text range (>= 0x1000):
            print()
            print("  First 8 events at PC >= 0x1000 (aspMain context):")
            print("    seq    pc      dma_mem  dma_dram      r28        r29     r31")
            count = 0
            for ev in boot:
                if int(ev["pc"]) >= 0x1000:
                    print(f"    {ev['seq']:6d}  "
                          f"0x{int(ev['pc']):04x}  "
                          f"0x{int(ev['dma_mem_addr']):04x}    "
                          f"0x{int(ev['dma_dram_addr']):08x}  "
                          f"0x{int(ev['gpr'][28]):08x} "
                          f"0x{int(ev['gpr'][29]):08x} "
                          f"0x{int(ev['gpr'][31]):04x}")
                    count += 1
                    if count >= 8:
                        break

        # Verdict.
        print()
        print("=" * 72)
        print("VERDICT")
        print("=" * 72)
        ares_dp = t["data_ptr"]
        ares_ds = t["data_size"]
        st_dp = STADIUM_HOOK["data_ptr"]
        st_ds = STADIUM_HOOK["data_size"]
        print(f"  Ares' OSTask:    data_ptr={fmt_addr(ares_dp):>12s}  "
              f"data_size=0x{ares_ds:04X}")
        print(f"  Stadium's hook:  data_ptr={fmt_addr(st_dp):>12s}  "
              f"data_size=0x{st_ds:04X}")
        if ares_dp == st_dp and ares_ds == st_ds:
            print()
            print("  *** OSTask MATCHES. Stadium and Ares process the same task.")
            print("  *** Hypothesis (input-state divergence) is REJECTED.")
            print("  *** Next step: trace r31 mutations in Ares to find what")
            print("      the recompiler emit is missing.")
        elif ares_dp == st_dp:
            print()
            print("  ! data_ptr matches but data_size differs. Stadium is")
            print(f"    processing 0x{st_ds:X} bytes, Ares 0x{ares_ds:X} bytes.")
            print("    Could be the same task at different scheduling points.")
        else:
            print()
            print("  *** OSTask DIFFERS. Stadium and Ares process different tasks.")
            print("  *** Hypothesis (input-state divergence) is CONFIRMED.")
            print("  *** Next step: identify which scheduler slot Stadium is")
            print("      registering and why it differs from Ares' selection.")

    finally:
        if proc and not args.keep_server:
            proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
