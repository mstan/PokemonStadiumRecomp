#!/usr/bin/env python3
# Reader for the always-on libnaudio voice-event ring (debug-server cmd
# `voice_events_recent`). Captures key-on / key-off / sample-change
# events emitted once per audio frame by librecomp's audio_uaf_protect
# voice ring.
#
# Built for the music-rate periodic click investigation (1-5 Hz,
# tempo-tracking). Prints each event plus a cadence summary so the
# voice-start rate can be compared against the audible click rate.
#
#   python tools/_voice_events.py [count] [--onsets]
#
#   count      number of most-recent events to fetch (default 256)
#   --onsets   restrict the cadence summary to onset events
#              (key_on + sample_change) — the click suspects
#
# Ring is ALWAYS-ON: this just queries backward over the window of
# interest. No arming. Disable the ring entirely with
# PSR_DISABLE_VOICE_RING=1 in the runner's environment.

import socket, json, sys

HOST, PORT = "127.0.0.1", 4371


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "voice_events_recent", "n": count}) + "\n").encode())
    buf = b""
    while b"\n" not in buf:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf.decode().strip())


def cadence(events, label):
    """Print inter-event timing stats over the host-ms timestamps."""
    if len(events) < 2:
        print(f"  [{label}] <2 events, no cadence")
        return
    ms = [e["ms"] for e in events]
    deltas = [b - a for a, b in zip(ms, ms[1:]) if b >= a]
    if not deltas:
        print(f"  [{label}] no positive deltas")
        return
    span_s = (ms[-1] - ms[0]) / 1000.0
    rate = (len(events) - 1) / span_s if span_s > 0 else float("nan")
    deltas_sorted = sorted(deltas)
    med = deltas_sorted[len(deltas_sorted) // 2]
    print(f"  [{label}] n={len(events)} span={span_s:.2f}s "
          f"rate={rate:.2f}/s  delta_ms: min={min(deltas)} "
          f"med={med} max={max(deltas)}")


def main():
    args = [a for a in sys.argv[1:]]
    onsets_only = "--onsets" in args
    args = [a for a in args if not a.startswith("--")]
    count = int(args[0]) if args else 256

    data = fetch(count)
    if not data.get("ok", False):
        print("ERROR:", data)
        return
    events = data.get("events", [])
    print(f"write_idx={data.get('write_idx')}  got={len(events)} events")

    for e in events:
        print(
            f"seq={e['seq']:6d} ms={e['ms']:8d} pass={e['pass']:6d} "
            f"{e['kind']:>13s} voice={e['voice']:#010x} "
            f"mot {e['prev_motion']}->{e['em_motion']} "
            f"tbl {e['prev_table']:#010x}->{e['dc_table']:#010x} "
            f"first={e['dc_first']} samp={e['dc_sample']} "
            f"last={e['dc_lastsam']} carry=({e['carry0']},{e['carry1']}) "
            f"wt(base={e['wt_base']:#010x} len={e['wt_len']} type={e['wt_type']}) "
            f"loop({e['loop_start']},{e['loop_end']},{e['loop_count']})"
        )

    print("\n=== cadence ===")
    cadence(events, "all")
    attacks = [e for e in events if e["kind"] == "attack"]
    reallocs = [e for e in events if e["kind"] == "realloc"]
    cadence(attacks, "attack (dc_first==1, true note onset)")
    cadence(reallocs, "realloc (voice reuse as CONTINUE)")
    cadence([e for e in events if e["kind"] == "key_off"], "key_off (churn)")

    # Stale-predictor smell #1: a true ATTACK (predictor SHOULD reset)
    # whose captured carry is non-zero — only matters if the RSP fails to
    # honor A_INIT, but worth surfacing.
    attack_carry = [e for e in attacks if e["carry0"] or e["carry1"]]
    if attack_carry:
        print(f"\n   {len(attack_carry)}/{len(attacks)} attacks have non-zero "
              f"predictor carry at samp==0 (benign if RSP honors A_INIT)")
    # Stale-predictor smell #2: REALLOC is by construction a CONTINUE with
    # dc_first==0; if the loaded predictor state belongs to the displaced
    # stream this is the click. These are the prime correlation targets.
    if reallocs:
        print(f"   {len(reallocs)} realloc events (CONTINUE onto a new "
              f"wavetable) — prime click-correlation candidates")


if __name__ == "__main__":
    main()
