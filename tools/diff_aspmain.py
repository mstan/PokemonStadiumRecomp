"""diff_aspmain.py — divergence diff between Stadium's recompiled
aspMain and Ares' RSP-side execution of the same ucode.

Why this exists:
    Stadium hangs in aspMain at 0x1038 ==> 0x103C ==> 0x1048 ==> 0x10EC
    (4-PC dispatch loop). Static analysis can't tell us *why* the
    cycle is structurally infinite — we need an oracle (Ares) running
    the same ROM to point at where the recompiled code first diverged
    from real RSP behavior.

How it works:
    1. Connect to Stadium's debug server (port 4371). Stadium is
       running its recompiled aspMain, which after ~200ms has entered
       the hang. Read the live pc_trail snapshot
       (ring of 32 most-recent PCs visited).
    2. Spawn ares_oracle_server (port 4372) on the same baserom and
       step Ares forward to the point where the *first* RSP task
       fires (--ares-frames, default 60 — matches engine smoke).
    3. Search Ares' RSP trace ring for the longest prefix of
       Stadium's pc_trail that appears verbatim. The first PC in
       Stadium's trail that does NOT appear in Ares' trace at the
       expected position is the divergence point.
    4. Dump Ares' GPR + DMA register snapshot at the divergence point
       so the user can see what the oracle expected vs what Stadium
       actually did.

Usage:
    python tools/diff_aspmain.py [--stadium-port 4371] [--ares-port 4372]
                                 [--rom baserom.z64] [--ares-frames 60]
                                 [--oracle-server <path>]

Stadium must already be running (the harness only orchestrates Ares).
"""
from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from contextlib import contextmanager
from typing import Optional

# Resolve defaults relative to this script so the tool isn't tied to any one
# machine's directory layout. tools/ -> repo root -> n64recomp workspace root.
_REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_ROOT = os.path.dirname(_REPO)
_DEFAULT_ROM = os.path.join(_REPO, "baserom.z64")
_DEFAULT_ORACLE = os.path.join(
    _ROOT, "N64Recomp", "build-aresbridge", "ares-bridge", "Release",
    "ares_oracle_server.exe")


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


def get_stadium_trail(stadium_port: int) -> list[int]:
    """Return Stadium's live pc_trail in chronological order
    (oldest first, newest last). Empty if not yet running a ucode."""
    with tcp_client("127.0.0.1", stadium_port) as (s, f):
        r = _send(s, f, "get_last_pc_trail")
        if not r.get("valid"):
            return []
        return [int(p) & 0xFFF for p in r.get("pc_trail", [])]


def step_oracle_to_state(ares_port: int, frames: int) -> dict:
    """Drive Ares forward by `frames` and return the final status."""
    with tcp_client("127.0.0.1", ares_port) as (s, f):
        # Confirm the server is alive.
        if not _send(s, f, "ping").get("ok"):
            raise RuntimeError("ares oracle ping failed")
        s.settimeout(120)
        r = _send(s, f, {"cmd": "step_frame", "n": frames})
        if not r.get("ok"):
            raise RuntimeError(f"step_frame failed: {r}")
        return _send(s, f, "status")


def fetch_oracle_recent_events(ares_port: int, n: int) -> list[dict]:
    with tcp_client("127.0.0.1", ares_port) as (s, f):
        s.settimeout(30)
        r = _send(s, f, {"cmd": "rsp_trace_recent", "n": n})
        return r.get("events", [])


def extract_label_entries(events: list[dict]) -> list[tuple[int, int]]:
    """From a per-instruction Ares trace, extract entries where a
    branch landed (i.e. the prior PC + 4 != current PC). Returns a
    list of (event_index_in_input, pc) so callers can recover the
    original GPR snapshot at each label.

    Stadium's pc_trail records labels (basic-block heads); aligning
    against Ares requires the same granularity. Without this filter,
    Ares' straight-line PCs (between labels) are noise that prevents
    contiguous matching."""
    out: list[tuple[int, int]] = []
    prev_pc: Optional[int] = None
    for i, ev in enumerate(events):
        pc = int(ev["pc"]) & 0xFFF
        if prev_pc is None or ((prev_pc + 4) & 0xFFF) != pc:
            out.append((i, pc))
        prev_pc = pc
    return out


def find_longest_match(stadium_trail: list[int],
                       ares_labels: list[tuple[int, int]]) -> tuple[int, int]:
    """Find the longest tail of stadium_trail that appears as a contiguous
    subsequence in ares_labels' PC sequence. Returns (match_length,
    index_into_ares_labels_of_first_match). If no match, returns (0, -1)."""
    if not stadium_trail or not ares_labels:
        return 0, -1
    ares_pcs_only = [pc for (_, pc) in ares_labels]
    for length in range(len(stadium_trail), 0, -1):
        needle = stadium_trail[-length:]
        for i in range(len(ares_pcs_only) - length + 1):
            if ares_pcs_only[i:i + length] == needle:
                return length, i
    return 0, -1


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--stadium-port", type=int, default=4371,
                    help="Stadium runner debug-server port (default 4371)")
    ap.add_argument("--ares-port", type=int, default=4372,
                    help="ares_oracle_server port (default 4372)")
    ap.add_argument("--rom", default=_DEFAULT_ROM,
                    help="Path to baserom.z64 (must match what Stadium uses)")
    ap.add_argument("--ares-frames", type=int, default=60,
                    help="Frames to step Ares forward before diffing")
    ap.add_argument("--oracle-server",
                    default=_DEFAULT_ORACLE,
                    help="Path to the ares_oracle_server.exe binary. If "
                         "the server is already running on --ares-port, "
                         "pass empty string to skip spawning.")
    args = ap.parse_args(argv)

    # Step 1: read Stadium's live trail.
    print(f"[1/5] Querying Stadium pc_trail on tcp:{args.stadium_port}...")
    trail = get_stadium_trail(args.stadium_port)
    if not trail:
        print("  Stadium reports no ucode running yet. Wait a few seconds "
              "after launch and rerun.")
        return 2
    print(f"  Stadium pc_trail (chronological, {len(trail)} entries):")
    for i, pc in enumerate(trail):
        marker = " <-- newest" if i == len(trail) - 1 else ""
        print(f"    [{i:2}] 0x{pc:04X}{marker}")

    # Step 2: spawn the oracle (or assume it's already up).
    proc: Optional[subprocess.Popen] = None
    if args.oracle_server:
        print(f"[2/5] Spawning oracle: {args.oracle_server}")
        proc = subprocess.Popen(
            [args.oracle_server, args.rom, str(args.ares_port)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
    else:
        print("[2/5] --oracle-server is empty; expecting an already-running "
              f"server on tcp:{args.ares_port}")

    try:
        # Step 3: advance Ares.
        print(f"[3/5] Stepping Ares forward {args.ares_frames} frames...")
        st = step_oracle_to_state(args.ares_port, args.ares_frames)
        print(f"  Ares trace_count = {st.get('trace_count')}")
        if st.get("trace_count", 0) == 0:
            print("  Oracle produced 0 events — RSP did not run in this "
                  "frame window. Try increasing --ares-frames.")
            return 3

        # Step 4: pull a chunk of recent oracle PCs to scan.
        print("[4/5] Fetching oracle's recent RSP trace (1024 events)...")
        events = fetch_oracle_recent_events(args.ares_port, 1024)
        ares_pcs = [int(e["pc"]) & 0xFFF for e in events]
        print(f"  Got {len(events)} events; PC range "
              f"0x{min(ares_pcs):04X}..0x{max(ares_pcs):04X}")

        ares_labels = extract_label_entries(events)
        print(f"  Filtered to {len(ares_labels)} label entries "
              f"(branch targets)")

        # Step 5: find divergence.
        print("[5/5] Aligning Stadium trail with oracle trace...")
        match_len, match_pos = find_longest_match(trail, ares_labels)

        if match_len == 0:
            print("\n=== NO MATCH IN LAST 1024 ARES EVENTS ===")
            print("  The window of recent Ares events doesn't contain "
                  "Stadium's trail. Querying Ares' full ring per-PC for "
                  "where each Stadium PC was seen:")
            unique_pcs = sorted(set(trail))
            seen = {}
            with tcp_client("127.0.0.1", args.ares_port) as (s, f):
                s.settimeout(30)
                for pc in unique_pcs:
                    r = _send(s, f, {"cmd": "rsp_trace_at_pc",
                                     "pc": pc, "n": 8})
                    seen[pc] = r.get("events", [])
                    n_hits = len(seen[pc])
                    if n_hits == 0:
                        print(f"    Stadium PC 0x{pc:04X}: NEVER seen "
                              f"in Ares' RSP trace.")
                    else:
                        seqs = [e["seq"] for e in seen[pc]]
                        print(f"    Stadium PC 0x{pc:04X}: seen "
                              f"{n_hits} times (recent seqs={seqs})")

            # Detect the "loop exits in Ares, repeats in Stadium" case.
            # Stadium's tail is a small cycle. Find the LAST occurrence
            # of the cycle's distinct PCs in Ares; if there's an Ares
            # event AFTER the last cycle iteration with a PC that's NOT
            # in the cycle, that PC is where Ares takes the exit
            # branch Stadium is missing.
            cycle_pcs = set(trail)
            last_cycle_seq = -1
            for pc, evs in seen.items():
                for ev in evs:
                    last_cycle_seq = max(last_cycle_seq, int(ev["seq"]))
            if last_cycle_seq >= 0:
                print()
                total_seq = st.get("trace_count", 0)
                print(f"  Latest Ares event in the cycle: seq={last_cycle_seq}")
                print(f"  Ares total events:              seq<{total_seq}")
                if total_seq - last_cycle_seq > 2:
                    print(f"  ==> Ares ran {total_seq - last_cycle_seq} more "
                          "instructions after the cycle ended. Stadium "
                          "would never reach those because it re-enters "
                          "the cycle.")
                    # Pull events right after the cycle exit to identify
                    # the next label Ares took.
                    with tcp_client("127.0.0.1", args.ares_port) as (s, f):
                        s.settimeout(30)
                        # rsp_trace_recent gives us the tail; we want
                        # the slice [last_cycle_seq+1, last_cycle_seq+64].
                        # The trace_recent API only returns the last N
                        # events, but last_cycle_seq is close to the end
                        # so the last 256 events should bracket the
                        # exit point.
                        r = _send(s, f, {"cmd": "rsp_trace_recent",
                                          "n": min(512, total_seq)})
                        post_events = [e for e in r.get("events", [])
                                       if int(e["seq"]) > last_cycle_seq]
                        if post_events:
                            # Find the first label entry (where prev_pc+4
                            # != cur_pc).
                            prev_pc = None
                            first_exit = None
                            for ev in post_events:
                                pc = int(ev["pc"]) & 0xFFF
                                if (prev_pc is not None and
                                        ((prev_pc + 4) & 0xFFF) != pc and
                                        pc not in cycle_pcs):
                                    first_exit = ev
                                    break
                                prev_pc = pc
                            if first_exit:
                                ev = first_exit
                                print()
                                print("  === ARES' EXIT BRANCH ===")
                                print(f"  Ares takes branch to PC "
                                      f"0x{int(ev['pc']):04X} at seq "
                                      f"{ev['seq']} (Stadium never "
                                      "reaches this PC).")
                                print(f"  Ares status at exit: "
                                      f"0x{int(ev['status']):08X}")
                                print("  Ares GPRs at exit (the values "
                                      "Stadium would need for the "
                                      "branch to fire):")
                                for reg in (1, 2, 3, 4, 5, 25, 26, 27,
                                            28, 29, 30, 31):
                                    print(f"    r{reg:<2} = 0x"
                                          f"{int(ev['gpr'][reg]):08X}")
                                print(f"  Ares dma_mem_addr=0x"
                                      f"{int(ev['dma_mem_addr']):08X} "
                                      f"dma_dram_addr=0x"
                                      f"{int(ev['dma_dram_addr']):08X}")
            return 4

        print(f"  Longest match: {match_len} of {len(trail)} entries, "
              f"starting at oracle index {match_pos}")

        if match_len == len(trail):
            print("\n=== NO DIVERGENCE DETECTED ===")
            print("  Stadium's full pc_trail matches Ares verbatim. The "
                  "hang may be a recompiler emit issue (e.g. infinite "
                  "loop emitted for a finite RSP loop) rather than a "
                  "control-flow divergence. Inspect Stadium's recompiled "
                  "aspMain.cpp around the matched PCs.")
            tail_pc = trail[-1]
            ares_label_pcs = [pc for (_, pc) in ares_labels]
            after = (ares_label_pcs[match_pos + match_len]
                     if match_pos + match_len < len(ares_label_pcs)
                     else None)
            print(f"  Stadium tail (last PC):    0x{tail_pc:04X}")
            if after is not None:
                print(f"  Ares NEXT label after match: 0x{after:04X}")
                print(f"  ==> Ares advances past 0x{tail_pc:04X} to "
                      f"0x{after:04X}; Stadium re-enters the loop instead.")
                # Show GPRs at the moment Ares took the divergent path
                # (the event right before the divergent label).
                ev_idx = ares_labels[match_pos + match_len][0]
                ev = events[ev_idx]
                print(f"  Ares GPRs at the divergent label "
                      f"(seq={ev['seq']}):")
                for r in (1, 2, 3, 4, 5, 25, 26, 27, 28, 29, 30, 31):
                    print(f"    r{r:<2} = 0x{int(ev['gpr'][r]):08X}")
                print(f"  Ares dma_mem_addr=0x{int(ev['dma_mem_addr']):08X}"
                      f" dma_dram_addr="
                      f"0x{int(ev['dma_dram_addr']):08X}")
            return 0

        # Partial match: Stadium and Ares agreed up to some point and
        # then diverged. Report the first divergent PC.
        diverge_idx_in_trail = len(trail) - match_len - 1
        if diverge_idx_in_trail < 0:
            print("  Match length exceeds trail length; logic error.")
            return 5
        stadium_pc_at_div = trail[diverge_idx_in_trail]

        # The matched run starts at ares_labels[match_pos]. The label
        # immediately before it (match_pos - 1) is what Ares took
        # before agreeing with Stadium's trail.
        oracle_label_at_div = (ares_labels[match_pos - 1]
                                if match_pos >= 1 else None)

        print("\n=== DIVERGENCE DETECTED ===")
        print(f"  Stadium PC at divergence:  0x{stadium_pc_at_div:04X}")
        if oracle_label_at_div:
            ev_idx, ev_pc = oracle_label_at_div
            ev = events[ev_idx]
            print(f"  Ares PC at divergence:     0x{ev_pc:04X}")
            print(f"  Ares seq at divergence:    {ev['seq']}")
            print(f"  Ares status:               0x{int(ev['status']):08X}")
            print("  Ares GPRs at divergence:")
            for r in (1, 2, 3, 4, 5, 25, 26, 27, 28, 29, 30, 31):
                print(f"    r{r:<2} = 0x{int(ev['gpr'][r]):08X}")
            print(f"  Ares dma_mem_addr=0x{int(ev['dma_mem_addr']):08X} "
                  f"dma_dram_addr=0x{int(ev['dma_dram_addr']):08X}")
        return 0
    finally:
        if proc is not None:
            try:
                with tcp_client("127.0.0.1", args.ares_port, attempts=2) \
                        as (s, f):
                    try:
                        _send(s, f, "quit")
                    except Exception:
                        pass
            except Exception:
                pass
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
