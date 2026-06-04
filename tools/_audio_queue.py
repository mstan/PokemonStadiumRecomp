#!/usr/bin/env python3
# Reader for the host audio-queue ring (debug-server cmd
# `audio_queue_recent`). One event per queue_samples() call on the host
# audio thread. Surfaces the SDL queue depth and whether the skip_factor
# sample-decimation fired -- the prime suspect for the music-rate click
# (dropping samples mid-waveform => discontinuity at chunk boundaries).
#
# Run with fast-forward OFF (PSR_TURBO=0) and volume up to reproduce the
# real-time listening condition. If `decimated` is ever 1 in real-time,
# the decimation path is active and is the likely click. If it is always
# 0, the click is elsewhere in queue_samples (e.g. the duplicated-frame
# carry-over / SDL_ConvertAudio resampling at chunk boundaries).
#
#   python tools/_audio_queue.py [count]

import os, socket, json, sys

HOST, PORT = "127.0.0.1", int(os.environ.get("PSR_DEBUG_PORT", "4371"))


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "audio_queue_recent", "n": count}) + "\n").encode())
    buf = b""
    while b"\n" not in buf:
        chunk = s.recv(65536)
        if not chunk:
            break
        buf += chunk
    s.close()
    return json.loads(buf.decode().strip())


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    data = fetch(count)
    if not data.get("ok", False):
        print("ERROR:", data)
        return
    events = data.get("events", [])
    decimated = [e for e in events if e["decimated"]]
    print(f"write_idx={data.get('write_idx')}  got={len(events)} chunks  "
          f"decimated={len(decimated)}")

    for e in events:
        tag = f"  <<< DECIMATED 1:{1 << e['skip_factor']}" if e["decimated"] else ""
        print(f"seq={e['seq']:6d} ms={e['ms']:8d} samps={e['sample_count']:5d} "
              f"rate={e['sample_rate']} queued={e['queued_us']:7d}us "
              f"skip={e['skip_factor']} bytes={e['bytes_queued']:6d}{tag}")

    if len(events) >= 2:
        ms = [e["ms"] for e in events]
        span = (ms[-1] - ms[0]) / 1000.0
        rate = (len(events) - 1) / span if span > 0 else float("nan")
        qmax = max(e["queued_us"] for e in events)
        qmin = min(e["queued_us"] for e in events)
        print(f"\nchunk rate={rate:.2f}/s span={span:.2f}s  "
              f"queue depth: min={qmin}us max={qmax}us")
        if decimated:
            print(f"!! {len(decimated)}/{len(events)} chunks decimated "
                  f"(skip_factor>0) — sample-dropping is active, prime click suspect")
        elif qmax > 100000:
            print("   queue exceeded 100ms but no decimation recorded "
                  "(check threshold logic)")
        else:
            print("   no decimation — queue stayed under the 100ms threshold; "
                  "click is NOT the skip_factor path")


if __name__ == "__main__":
    main()
