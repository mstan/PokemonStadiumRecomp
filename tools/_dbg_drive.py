"""TCP-driven button-press harness for PokemonStadiumRecomp.

Removes the human-in-the-loop dependency for repro of menu-load crash
and similar input-triggered failures. Connects to the runner's debug
server (TCP 127.0.0.1:4371), waits for boot via status polling, sends
a configurable button sequence, and captures last_error.log +
trace_recent + libultra_recent on crash.

Per the global "always-on ring buffer" rule: this harness QUERIES the
trace rings post-fact. It does not arm/capture/dump.

Usage:

  # Default: launch the runner, wait for title, press Start three
  # times (the known crash trigger), save report to last_drive.json:
  python tools/_dbg_drive.py

  # Connect to an already-running runner instead of launching:
  python tools/_dbg_drive.py --no-launch

  # Custom sequence — comma-separated <button>:<down_ms>:<gap_ms>
  # tokens. Buttons match debug_server.cpp: A B Z Start DUp DDown
  # DLeft DRight L R CUp CDown CLeft CRight.
  python tools/_dbg_drive.py --seq "Start:100:1500,Start:100:1500,A:100:500"

  # Stick override: x,y in [-128,127], hold for ms.
  python tools/_dbg_drive.py --stick "60,0,2000"
"""
from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path("F:/Projects/PokemonStadiumRecomp")
RUNNER_EXE = ROOT / "build" / "PokemonStadiumRecomp.exe"
ERRLOG_PATH = ROOT / "build" / "last_error.log"
HOST = "127.0.0.1"
PORT = 4371

# Default crash repro: three Start presses on the title screen, with
# enough gap between presses for the runner to schedule a frame.
DEFAULT_SEQUENCE = "Start:100:1500,Start:100:1500,Start:100:1500"


def call(cmd: str | dict, timeout: float = 4.0) -> dict[str, Any]:
    """Send one command, return parsed JSON or {"_err": ...}."""
    if isinstance(cmd, dict):
        payload = json.dumps(cmd)
    else:
        payload = cmd
    try:
        s = socket.create_connection((HOST, PORT), timeout=timeout)
    except OSError as e:
        return {"_err": f"connect: {e}"}
    try:
        s.sendall((payload + "\n").encode())
        s.settimeout(timeout)
        buf = b""
        while True:
            try:
                c = s.recv(65536)
            except socket.timeout:
                break
            if not c:
                break
            buf += c
            if buf.endswith(b"\n"):
                break
    finally:
        s.close()
    text = buf.decode(errors="replace").strip()
    if not text:
        return {"_err": "empty response"}
    try:
        return json.loads(text)
    except Exception as e:
        return {"_err": f"json: {e}", "_raw": text}


def wait_for_boot(target_frame: int = 60, timeout_s: float = 90.0) -> dict[str, Any]:
    """Block until status reports frame >= target_frame, or timeout.

    Returns the final status dict (with "_err" key on failure).
    """
    deadline = time.time() + timeout_s
    last = {}
    while time.time() < deadline:
        last = call({"cmd": "status"}, timeout=2.0)
        frame = last.get("frame", 0) if isinstance(last.get("frame"), int) else 0
        if frame >= target_frame:
            return last
        time.sleep(0.5)
    last["_err"] = f"boot timeout after {timeout_s}s; last frame={last.get('frame')}"
    return last


def parse_sequence(spec: str) -> list[tuple[str, int, int]]:
    """Parse "Start:100:1500,A:80:500" into [(name, down_ms, gap_ms), ...]."""
    out: list[tuple[str, int, int]] = []
    for token in spec.split(","):
        token = token.strip()
        if not token:
            continue
        parts = token.split(":")
        if len(parts) != 3:
            raise ValueError(f"bad token {token!r}; expected NAME:DOWN_MS:GAP_MS")
        name, down_ms, gap_ms = parts[0], int(parts[1]), int(parts[2])
        out.append((name, down_ms, gap_ms))
    return out


def press(name: str, down_ms: int) -> None:
    call({"cmd": "set_button", "name": name, "down": True})
    time.sleep(down_ms / 1000.0)
    call({"cmd": "set_button", "name": name, "down": False})


def gather_diagnostics() -> dict[str, Any]:
    """Read everything that helps post-mortem on a hung/crashed runner."""
    return {
        "status": call({"cmd": "status"}, timeout=2.0),
        "trace_recent": call({"cmd": "trace_recent", "n": 128}, timeout=4.0),
        "libultra_recent": call({"cmd": "libultra_recent", "n": 64}, timeout=4.0),
        "last_pc_trail": call({"cmd": "get_last_pc_trail"}, timeout=2.0),
        "tail_errlog": call({"cmd": "tail_errlog"}, timeout=4.0),
    }


def read_errlog_tail(max_bytes: int = 16 * 1024) -> str:
    """Direct-read the last_error.log file (server may be dead)."""
    if not ERRLOG_PATH.exists():
        return ""
    try:
        with ERRLOG_PATH.open("rb") as f:
            f.seek(0, 2)
            size = f.tell()
            f.seek(max(0, size - max_bytes))
            return f.read().decode(errors="replace")
    except OSError as e:
        return f"<read error: {e}>"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--no-launch", action="store_true",
                   help="Connect to an already-running runner instead of spawning one")
    p.add_argument("--seq", default=DEFAULT_SEQUENCE,
                   help="Button sequence: comma-separated NAME:DOWN_MS:GAP_MS")
    p.add_argument("--stick", default=None,
                   help="Stick override before button sequence: X,Y,HOLD_MS")
    p.add_argument("--boot-frame", type=int, default=120,
                   help="Wait until status.frame >= this before pressing")
    p.add_argument("--boot-timeout", type=float, default=90.0,
                   help="Max seconds to wait for boot")
    p.add_argument("--post-wait", type=float, default=3.0,
                   help="Seconds to idle after sequence before gathering diagnostics")
    p.add_argument("--report", default=str(ROOT / "build" / "last_drive.json"),
                   help="Path for JSON report")
    p.add_argument("--quit-after", action="store_true",
                   help="Send {cmd:quit} after diagnostics (clean shutdown)")
    p.add_argument("--peek", action="append", default=[],
                   help="rdram_peek spec: ADDR:N (hex addr, decimal bytes). "
                        "Repeatable. Read post-sequence (before crash, if any).")
    p.add_argument("--runner-stderr", default=str(ROOT / "build" / "drive_runner.stderr"),
                   help="When launching, redirect runner stderr here")
    args = p.parse_args()

    sequence = parse_sequence(args.seq)
    stick: tuple[int, int, int] | None = None
    if args.stick:
        sx, sy, hold = (int(v) for v in args.stick.split(","))
        stick = (sx, sy, hold)

    proc: subprocess.Popen | None = None
    if not args.no_launch:
        if not RUNNER_EXE.exists():
            print(f"[drive] runner missing: {RUNNER_EXE}", file=sys.stderr)
            return 2
        # Truncate the errlog so we can attribute new entries to this run.
        try:
            ERRLOG_PATH.write_text("")
        except OSError:
            pass
        stderr_log = open(args.runner_stderr, "w", encoding="utf-8", errors="replace")
        print(f"[drive] launching {RUNNER_EXE.name}; stderr -> {args.runner_stderr}")
        proc = subprocess.Popen(
            [str(RUNNER_EXE)],
            cwd=str(RUNNER_EXE.parent),
            stdout=subprocess.DEVNULL,
            stderr=stderr_log,
            stdin=subprocess.DEVNULL,
        )

    report: dict[str, Any] = {
        "args": vars(args),
        "sequence": sequence,
        "stick": stick,
        "events": [],
    }

    def event(kind: str, **data: Any) -> None:
        rec = {"t": round(time.time() - t0, 3), "kind": kind, **data}
        report["events"].append(rec)
        print(f"[drive] {kind}: {json.dumps(data, default=str)}")

    t0 = time.time()
    exit_code = 0

    try:
        # Connection retry — the server takes a moment to bind.
        connected = False
        deadline = time.time() + 30.0
        while time.time() < deadline:
            r = call({"cmd": "ping"}, timeout=1.0)
            if r.get("ok"):
                connected = True
                break
            if proc is not None and proc.poll() is not None:
                event("runner_exited_early", returncode=proc.returncode)
                report["fatal"] = "runner exited before TCP server came up"
                exit_code = 3
                break
            time.sleep(0.5)
        if not connected:
            if "fatal" not in report:
                report["fatal"] = "TCP server never accepted ping"
                exit_code = 3
            raise SystemExit(exit_code)

        event("connected")

        # Arm the input override IMMEDIATELY so libultra's osContInit
        # (one-shot during boot) sees a controller present in port 1.
        # Without this, the game reports "Controller 1 not connected"
        # and ignores everything we send via set_button afterward.
        claim = call({"cmd": "claim_input"}, timeout=2.0)
        event("claim_input", **claim)

        booted = wait_for_boot(args.boot_frame, args.boot_timeout)
        event("boot_status", **booted)
        if booted.get("_err"):
            report["fatal"] = booted["_err"]
            exit_code = 4
            raise SystemExit(exit_code)

        if stick is not None:
            sx, sy, hold = stick
            call({"cmd": "set_stick", "x": sx, "y": sy})
            event("stick_set", x=sx, y=sy)
            time.sleep(hold / 1000.0)
            call({"cmd": "set_stick", "x": 0, "y": 0})

        for i, (name, down_ms, gap_ms) in enumerate(sequence):
            # Detect mid-sequence crash early.
            if proc is not None and proc.poll() is not None:
                event("runner_died_mid_sequence", at_step=i, returncode=proc.returncode)
                break
            press(name, down_ms)
            event("press", step=i, name=name, down_ms=down_ms)
            time.sleep(gap_ms / 1000.0)

        time.sleep(args.post_wait)
        call({"cmd": "clear_input"})

        # Pre-crash peeks. Must run BEFORE diagnostics (which themselves
        # may fail if the runner died). On crash these will silently fail.
        peek_results: list[dict[str, Any]] = []
        for spec in args.peek:
            try:
                a_str, n_str = spec.split(":")
                addr = int(a_str, 0)
                n = int(n_str, 0)
            except Exception:
                event("peek_parse_error", spec=spec)
                continue
            r = call({"cmd": "rdram_peek", "addr": addr, "n": n}, timeout=2.0)
            r["_spec"] = spec
            peek_results.append(r)
            event("peek", spec=spec, hex=r.get("hex"), err=r.get("error"))
        report["peeks"] = peek_results

        # Diagnostics — try TCP first, fall back to file read for errlog
        # if the server is unresponsive (likely if the runner crashed).
        diag = gather_diagnostics()
        report["diagnostics"] = diag
        report["errlog_tail_file"] = read_errlog_tail()

        if proc is not None:
            rc = proc.poll()
            event("runner_state", returncode=rc, alive=rc is None)
            if rc is not None:
                report["runner_exit_code"] = rc

        if args.quit_after:
            call({"cmd": "quit"}, timeout=1.0)

    except SystemExit as e:
        exit_code = int(e.code) if e.code is not None else 0
    finally:
        if proc is not None:
            try:
                if proc.poll() is None:
                    proc.terminate()
                    try:
                        proc.wait(timeout=5.0)
                    except subprocess.TimeoutExpired:
                        proc.kill()
            except Exception:
                pass

        report_path = Path(args.report)
        report_path.parent.mkdir(parents=True, exist_ok=True)
        report_path.write_text(json.dumps(report, indent=2, default=str))
        print(f"[drive] report -> {report_path}")
        # Print a short summary of the errlog if there's anything in it.
        tail = report.get("errlog_tail_file") or ""
        if tail.strip():
            print("[drive] last_error.log tail (truncated to 4 KB):")
            print(tail[-4096:])

    return exit_code


if __name__ == "__main__":
    sys.exit(main())
