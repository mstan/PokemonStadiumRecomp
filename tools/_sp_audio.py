#!/usr/bin/env python3
# Reader for the SP-task ring (debug-server cmd `sp_task_recent`),
# filtered to M_AUDTASK submissions. Establishes the audio-CHUNK
# cadence ground truth: every aspMain audio frame is one M_AUDTASK,
# so the inter-task delta is the chunk boundary rate.
#
# Used alongside tools/_voice_events.py to disambiguate the music-rate
# click: if the click cadence matches the chunk (M_AUDTASK) rate it is
# a chunk-boundary / reseed bug; if it matches the voice onset rate it
# is a voice-start bug. Per the converged hypothesis the chunk rate is
# fixed (~30/60 Hz) and does NOT tempo-track, so this is the control.
#
#   python tools/_sp_audio.py [count]
#
#   count   number of most-recent SP tasks to fetch (default 256);
#           audio tasks are filtered out of that window.

import socket, json, sys

HOST, PORT = "127.0.0.1", 4371


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "sp_task_recent", "n": count}) + "\n").encode())
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
        print(f"  [{label}] <2 events, no cadence")
        return
    ms = [e["ms"] for e in events]
    deltas = [b - a for a, b in zip(ms, ms[1:]) if b >= a]
    if not deltas:
        print(f"  [{label}] no positive deltas")
        return
    span_s = (ms[-1] - ms[0]) / 1000.0
    rate = (len(events) - 1) / span_s if span_s > 0 else float("nan")
    sd = sorted(deltas)
    print(f"  [{label}] n={len(events)} span={span_s:.2f}s "
          f"rate={rate:.2f}/s  delta_ms: min={min(deltas)} "
          f"med={sd[len(sd) // 2]} max={max(deltas)}")


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 256
    data = fetch(count)
    if not data.get("ok", False):
        print("ERROR:", data)
        return
    events = data.get("events", [])
    audio = [e for e in events if e.get("type") == "M_AUDTASK"]
    print(f"write_idx={data.get('write_idx')}  got={len(events)} tasks "
          f"({len(audio)} audio)")

    for e in audio:
        print(
            f"seq={e['seq']:6d} ms={e['ms']:8d} frame={e['frame']:6d} "
            f"data_ptr={e['data_ptr']:#010x} size={e['data_size']} "
            f"out={e['output_buff']:#010x} ucode={e['ucode']:#010x}"
        )

    print("\n=== chunk cadence (M_AUDTASK) ===")
    cadence(audio, "M_AUDTASK")


if __name__ == "__main__":
    main()
