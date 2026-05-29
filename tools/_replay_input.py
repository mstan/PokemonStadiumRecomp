"""Replay a recorded input history (build/last_run_input_history.json).

Each set_button/set_stick/claim/clear event is replayed in order with
the same inter-event delays (timestamp_ms) as the original session.
Lets us re-trigger an input-induced crash without a human in the loop.

Usage:

  # Launch runner + replay last session's input:
  python tools/_replay_input.py

  # Connect to an already-running runner (don't launch):
  python tools/_replay_input.py --no-launch

  # Use a different history file:
  python tools/_replay_input.py --history other.json

  # Speed multiplier on inter-event delays (1.0 = original timing).
  # Use < 1.0 to play back faster, > 1.0 to slow down.
  python tools/_replay_input.py --speed 1.0

  # After the last event, idle this many seconds, then dump diagnostics
  # and exit. Set to 0 to exit immediately.
  python tools/_replay_input.py --post-wait 5
"""
from __future__ import annotations

import argparse
import json
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path("F:/Projects/n64recomp/PokemonStadiumRecomp")
RUNNER_EXE = ROOT / "build" / "PokemonStadiumRecomp.exe"
HISTORY_PATH = ROOT / "build" / "last_run_input_history.json"
HOST = "127.0.0.1"
PORT = 4371


def call(cmd: dict | str, timeout: float = 4.0) -> dict[str, Any]:
    payload = json.dumps(cmd) if isinstance(cmd, dict) else cmd
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
        return {"_err": "empty"}
    try:
        return json.loads(text)
    except Exception as e:
        return {"_err": f"json: {e}", "_raw": text}


def wait_for_boot(target_frame: int, timeout_s: float) -> dict[str, Any]:
    deadline = time.time() + timeout_s
    last: dict[str, Any] = {}
    while time.time() < deadline:
        last = call({"cmd": "status"}, timeout=2.0)
        frame = last.get("frame", 0) if isinstance(last.get("frame"), int) else 0
        if frame >= target_frame:
            return last
        time.sleep(0.5)
    last["_err"] = f"boot timeout after {timeout_s}s"
    return last


def replay_event(ev: dict[str, Any]) -> dict[str, Any]:
    kind = ev.get("kind", "")
    if kind == "claim":
        return call({"cmd": "claim_input"})
    if kind == "clear":
        return call({"cmd": "clear_input"})
    if kind == "button":
        return call({"cmd": "set_button",
                     "name": ev.get("name", ""),
                     "down": bool(ev.get("down", 0))})
    if kind == "stick":
        return call({"cmd": "set_stick",
                     "x": int(ev.get("x", 0)),
                     "y": int(ev.get("y", 0))})
    return {"_err": f"unknown kind {kind!r}"}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--history", default=str(HISTORY_PATH),
                   help="Input history JSON file to replay")
    p.add_argument("--no-launch", action="store_true",
                   help="Connect to running runner instead of launching")
    p.add_argument("--speed", type=float, default=1.0,
                   help="Inter-event delay multiplier (1.0=original)")
    p.add_argument("--boot-frame", type=int, default=120,
                   help="Frame counter to wait for before replaying")
    p.add_argument("--boot-timeout", type=float, default=90.0)
    p.add_argument("--post-wait", type=float, default=5.0,
                   help="Idle this many seconds after last event")
    p.add_argument("--runner-stderr",
                   default=str(ROOT / "build" / "replay_runner.stderr"),
                   help="When launching, redirect runner stderr here")
    args = p.parse_args()

    history_path = Path(args.history)
    if not history_path.exists():
        print(f"[replay] history missing: {history_path}", file=sys.stderr)
        return 2
    history = json.loads(history_path.read_text())
    events = history.get("events", [])
    if not events:
        print("[replay] history has no events; nothing to replay", file=sys.stderr)
        return 2
    print(f"[replay] {len(events)} events from {history_path}; "
          f"speed={args.speed}; total={history.get('total_events')}")

    proc: subprocess.Popen | None = None
    if not args.no_launch:
        if not RUNNER_EXE.exists():
            print(f"[replay] runner missing: {RUNNER_EXE}", file=sys.stderr)
            return 2
        stderr_log = open(args.runner_stderr, "w", encoding="utf-8", errors="replace")
        print(f"[replay] launching {RUNNER_EXE.name}; stderr -> {args.runner_stderr}")
        proc = subprocess.Popen(
            [str(RUNNER_EXE)],
            cwd=str(RUNNER_EXE.parent),
            stdout=subprocess.DEVNULL,
            stderr=stderr_log,
            stdin=subprocess.DEVNULL,
        )

    try:
        # Wait for TCP server.
        deadline = time.time() + 30.0
        while time.time() < deadline:
            r = call({"cmd": "ping"}, timeout=1.0)
            if r.get("ok"):
                break
            if proc is not None and proc.poll() is not None:
                print(f"[replay] runner exited early: rc={proc.returncode}",
                      file=sys.stderr)
                return 3
            time.sleep(0.5)

        # Replay first event MUST be claim_input (or a stick/button reaching
        # boot). If history starts with claim, send it before booting so
        # osContInit sees the controller.
        first = events[0]
        if first.get("kind") == "claim":
            replay_event(first)
            print(f"[replay] sent claim_input (pre-boot)")
            events_to_play = events[1:]
            t_origin = first.get("t_ms", 0)
        else:
            events_to_play = events
            t_origin = 0

        booted = wait_for_boot(args.boot_frame, args.boot_timeout)
        if booted.get("_err"):
            print(f"[replay] boot failed: {booted['_err']}", file=sys.stderr)
            return 4
        print(f"[replay] booted; frame={booted.get('frame')}")

        # Walk events, sleeping the original deltas.
        t_prev = t_origin
        wall_t0 = time.time()
        for i, ev in enumerate(events_to_play):
            t_ms = ev.get("t_ms", t_prev)
            delta_ms = max(0, t_ms - t_prev) * args.speed
            if delta_ms > 0:
                time.sleep(delta_ms / 1000.0)
            r = replay_event(ev)
            ok = "ok" if r.get("ok") else "ERR"
            kind = ev.get("kind", "?")
            extra = ""
            if kind == "button":
                extra = f" {ev.get('name')} down={ev.get('down')}"
            elif kind == "stick":
                extra = f" x={ev.get('x')} y={ev.get('y')}"
            print(f"[replay] [{i+1}/{len(events_to_play)}] "
                  f"t={t_ms}ms kind={kind}{extra} -> {ok}")
            t_prev = t_ms

            # Detect mid-replay crash early.
            if proc is not None and proc.poll() is not None:
                print(f"[replay] runner died at event {i+1}; "
                      f"rc={proc.returncode}", file=sys.stderr)
                break

        # Idle so the crash (if input-triggered + delayed) lands.
        if args.post_wait > 0:
            print(f"[replay] post-wait {args.post_wait}s")
            time.sleep(args.post_wait)

        # Diagnostics dump.
        st = call({"cmd": "status"}, timeout=2.0)
        print(f"[replay] final status: {st}")
        if proc is not None:
            rc = proc.poll()
            print(f"[replay] runner alive={rc is None} rc={rc}")
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

    return 0


if __name__ == "__main__":
    sys.exit(main())
