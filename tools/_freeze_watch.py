"""Detect a white-screen / game-thread softlock in real time and trigger
post_mortem_dump over TCP while the runner is still alive.

Polls debug_server every 10s. The freeze signature (per
project_white_screen_rt64_hang_2026_05_03.md):
  - frame counter still advancing (game thread alive)
  - send_dl flatlined (no DLs being submitted)

When detected: send {"cmd":"post_mortem_dump"} so the unified
last_run_report.json captures the game-thread / Gfx-thread call stack
inside the wait loop. Print a marker to stdout for the operator.

Exits when:
  - dump triggered (success)
  - runner not reachable for 30s (assumed exited)
  - no freeze detected after MAX_POLLS polls

Usage: python tools/_freeze_watch.py
"""
from __future__ import annotations

import json
import socket
import sys
import time

ADDR = ("127.0.0.1", 4371)
POLL_INTERVAL_S = 10
MAX_POLLS = 60                       # 10 min ceiling
FREEZE_FRAMES_REQUIRED = 3           # send_dl flat for N polls (~30s)


def query(cmd: dict, timeout: float = 3.0) -> dict | None:
    try:
        s = socket.create_connection(ADDR, timeout=timeout)
    except (OSError, ConnectionRefusedError):
        return None
    try:
        s.sendall((json.dumps(cmd) + "\n").encode())
        buf = b""
        while True:
            c = s.recv(65536)
            if not c:
                break
            buf += c
            if buf.endswith(b"\n"):
                break
        return json.loads(buf.decode().strip())
    except Exception:
        return None
    finally:
        s.close()


def main() -> int:
    print(f"[watcher] start, poll every {POLL_INTERVAL_S}s")
    last_send_dl = -1
    last_frame = -1
    flat_polls = 0
    unreachable_polls = 0

    for poll in range(MAX_POLLS):
        d = query({"cmd": "status"})
        if d is None:
            unreachable_polls += 1
            print(f"[watcher] poll {poll}: runner unreachable ({unreachable_polls}/3)")
            if unreachable_polls >= 3:
                print("[watcher] runner gone — exiting")
                return 1
            time.sleep(POLL_INTERVAL_S)
            continue
        unreachable_polls = 0

        frame = int(d.get("frame", 0))
        send_dl = int(d.get("send_dl", 0))
        sub_gfx = int(d.get("submit_gfx", 0))
        dp = int(d.get("dp_complete", 0))
        print(f"[watcher] poll {poll}: frame={frame} send_dl={send_dl} "
              f"submit_gfx={sub_gfx} dp_complete={dp} flat={flat_polls}")

        if last_send_dl >= 0 and last_frame >= 0:
            frame_advanced = frame > last_frame
            send_dl_flat = send_dl == last_send_dl
            if frame_advanced and send_dl_flat:
                flat_polls += 1
            else:
                flat_polls = 0
        last_send_dl = send_dl
        last_frame = frame

        if flat_polls >= FREEZE_FRAMES_REQUIRED:
            print(f"[watcher] FREEZE DETECTED — frame advancing, "
                  f"send_dl flat at {send_dl}. Triggering post_mortem_dump.")
            r = query({"cmd": "post_mortem_dump"}, timeout=15.0)
            print(f"[watcher] dump response: {r!r}")
            return 0

        time.sleep(POLL_INTERVAL_S)

    print("[watcher] no freeze after max polls")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
