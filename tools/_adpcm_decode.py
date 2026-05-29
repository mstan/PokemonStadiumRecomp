#!/usr/bin/env python3
# Reader for the ADPCM-decode command-scan ring (debug-server cmd
# `adpcm_decode_recent`). Each event is one A_ADPCM command parsed out of
# the audio command list at submit time, carrying the GROUND-TRUTH
# A_INIT flag the RSP will act on -- the decision the task-time voice
# ring cannot observe (n_alAdpcmPull clears dc_first inside the frame).
#
# The click hypothesis: a CONTINUE decode (A_INIT clear) onto a book that
# changed vs the previous decode on the same predictor-state buffer loads
# the WRONG stream's predictor -> discontinuity -> audible click. Those
# are flagged `suspect=1`. This tool reports the suspect rate so it can be
# compared against the audible click rate.
#
#   python tools/_adpcm_decode.py [count] [--suspects]
#
#   count        number of most-recent decodes to fetch (default 4000)
#   --suspects   print only the suspect (stale-predictor) decodes

import socket, json, sys

HOST, PORT = "127.0.0.1", 4371


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "adpcm_decode_recent", "n": count}) + "\n").encode())
    buf = b""
    while b"\n" not in buf:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf.decode().strip())


def cadence(events, label):
    if len(events) < 2:
        print(f"  [{label}] n={len(events)} <2 events, no cadence")
        return
    ms = [e["ms"] for e in events]
    deltas = [b - a for a, b in zip(ms, ms[1:]) if b >= a]
    span_s = (ms[-1] - ms[0]) / 1000.0
    rate = (len(events) - 1) / span_s if span_s > 0 else float("nan")
    sd = sorted(deltas) if deltas else [0]
    print(f"  [{label}] n={len(events)} span={span_s:.2f}s rate={rate:.2f}/s "
          f"delta_ms: min={min(sd)} med={sd[len(sd) // 2]} max={max(sd)}")


def main():
    args = sys.argv[1:]
    suspects_only = "--suspects" in args
    args = [a for a in args if not a.startswith("--")]
    count = int(args[0]) if args else 4000

    data = fetch(count)
    if not data.get("ok", False):
        print("ERROR:", data)
        return
    events = data.get("events", [])
    inits = [e for e in events if e["init"]]
    conts = [e for e in events if not e["init"]]
    suspects = [e for e in events if e["suspect"]]

    print(f"write_idx={data.get('write_idx')}  got={len(events)} decodes  "
          f"(INIT={len(inits)} CONTINUE={len(conts)} SUSPECT={len(suspects)})")

    show = suspects if suspects_only else events
    for e in show:
        tag = " <<< SUSPECT" if e["suspect"] else ""
        print(f"seq={e['seq']:7d} ms={e['ms']:8d} frame={e['frame']:6d} "
              f"state={e['state']:#08x} book={e['book']:#08x} "
              f"prev_book={e['prev_book']:#08x} "
              f"{'INIT ' if e['init'] else 'CONT '} cnt={e['count']:4d}{tag}")

    print("\n=== cadence ===")
    cadence(events, "all decodes")
    cadence(inits, "INIT (true attack, predictor reset)")
    cadence(suspects, "SUSPECT (CONTINUE onto new book = stale predictor)")
    if suspects:
        print(f"\n!! {len(suspects)} stale-predictor decodes "
              f"({len(suspects)}/{len(events)} of all) — compare this rate "
              f"to the audible click rate")
    else:
        print("\n   no SUSPECT decodes in this window — predictor is reset "
              "(A_INIT) on every sample change")


if __name__ == "__main__":
    main()
