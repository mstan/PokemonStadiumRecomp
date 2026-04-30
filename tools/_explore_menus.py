"""Autonomously explore Stadium menus by reading the trace ring.

Strategy:
  - Sample trace_recent → extract recent fragment_entry symbols
    (anything matching "fragment\\d+_entry" or "frag_*").
  - This is the "menu signature" — which fragment is currently active.
  - Press a button, wait, sample again. If signature changed, log
    transition. If not, try a different button.

Uses a depth-limited DFS over button choices to find a path from
AREA_SELECT toward "1P battle → 3 pokemon → start match" (which the
user reports crashes the runner).

Stops on crash (last_error.log appears).
"""
from __future__ import annotations
import json, os, socket, sys, time, re
from pathlib import Path

ROOT = Path("F:/Projects/PokemonStadiumRecomp")
ERRLOG = ROOT / "build" / "last_error.log"

def call(c, t=3):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=t)
    s.sendall((c + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        x = s.recv(65536)
        if not x: break
        buf += x
    s.close()
    return json.loads(buf.decode().strip())

def state():
    r = call(json.dumps({"cmd":"rdram_peek","addr":0x80075668,"n":4}))
    return r.get("hex","?")

def trace_tail(n=200):
    r = call("trace_recent")
    ev = r.get("events", [])
    return ev[-n:] if isinstance(ev, list) else []

def signature(events):
    """Set of distinct fragment names in the trace tail."""
    out = set()
    for e in events:
        name = e if isinstance(e, str) else e.get("func", "")
        if "fragment" in name and "_entry" in name:
            out.add(name)
        elif name.startswith("frag_"):
            # vram-keyed fragments (e.g., frag_8FF00000__rom_xxxxxx_entry)
            short = name.split("__")[0]
            out.add(short)
    return out

def press(name, ms=120):
    call(json.dumps({"cmd":"set_button","name":name,"down":True}))
    time.sleep(ms/1000.0)
    call(json.dumps({"cmd":"set_button","name":name,"down":False}))

def crashed():
    return ERRLOG.exists()

# ── boot up to AREA_SELECT ────────────────────────────────────────
call(json.dumps({"cmd":"claim_input"}))
for _ in range(60):
    s = call("status")
    if s.get("frame", 0) >= 120: break
    time.sleep(0.3)
print(f"[boot] frame={s.get('frame')} state={state()}")

# Press 1 (intro -> title)
press("Start", 200); time.sleep(2.0)
print(f"[after press1] state={state()}")
# Press 2 (title -> area_select)
press("Start", 200); time.sleep(2.0)
print(f"[after press2] state={state()}")

if state().lower() != "00000004":
    print("WARNING: did not reach AREA_SELECT — aborting", file=sys.stderr)
    sys.exit(1)

if crashed():
    print("crash detected before navigation began", file=sys.stderr)
    sys.exit(2)

# ── snapshot signature at AREA_SELECT ─────────────────────────────
time.sleep(0.5)
sig0 = signature(trace_tail(200))
print(f"[area_select] fragments active: {sorted(sig0)}")

# ── try buttons one by one, observe transitions ───────────────────
TRIES = ["A", "Start", "DDown", "DUp", "DRight", "DLeft", "B"]
for btn in TRIES:
    if crashed():
        print(f"crash after trying earlier button — stopping", file=sys.stderr)
        break
    press(btn, 150)
    time.sleep(1.5)
    sig = signature(trace_tail(200))
    new = sig - sig0
    gone = sig0 - sig
    print(f"[press {btn}] state={state()} new_fragments={sorted(new)} gone={sorted(gone)}")
    if crashed():
        print(f"!!! crash after pressing {btn}")
        break
    sig0 = sig
