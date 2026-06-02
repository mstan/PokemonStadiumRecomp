#!/usr/bin/env python3
# Reader for the synthesized-PCM ring (debug-server cmd `audio_pcm_recent`).
# One event per queue_samples() call, capturing the RAW int16 audio the game
# synthesized BEFORE host conversion/volume. Metrics are on one channel.
#
# Static localizer: HF ratio = mean_abs_d2 / mean_abs.
#   - Clean tonal audio: 2nd-difference is small vs amplitude (ratio well < 1).
#   - Noise/static:      2nd-difference ~ amplitude (ratio ~1 or higher).
# If the HF ratio is high even on quiet/tonal music, the static is baked into
# the synthesized PCM (aspMain / mixing), not the host path.
#
#   python tools/_audio_pcm.py [count] [--windows N]

import socket, json, sys

HOST, PORT = "127.0.0.1", 4371


def fetch(count):
    s = socket.socket()
    s.connect((HOST, PORT))
    s.sendall((json.dumps({"cmd": "audio_pcm_recent", "n": count}) + "\n").encode())
    buf = b""
    while b"\n" not in buf:
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())


def main():
    args = sys.argv[1:]
    nwin = 0
    if "--windows" in args:
        i = args.index("--windows")
        nwin = int(args[i + 1])
        del args[i:i + 2]
    count = int(args[0]) if args else 300

    d = fetch(count)
    if not d.get("ok", False):
        print("ERROR:", d)
        return
    ev = d.get("events", [])
    print(f"write_idx={d.get('write_idx')}  got={len(ev)} chunks")
    if not ev:
        return

    def hf(e):
        return e["mean_abs_d2"] / e["mean_abs"] if e["mean_abs"] else 0.0

    ratios = [hf(e) for e in ev]
    audible = [e for e in ev if e["mean_abs"] > 200]  # ignore near-silence
    aud_ratios = [hf(e) for e in audible]

    print("\nper-chunk (audible chunks, mean_abs>200):")
    print(f"{'seq':>7} {'ms':>8} {'samps':>6} {'min':>7} {'max':>7} "
          f"{'mean|x|':>8} {'mean|d1|':>9} {'mean|d2|':>9} {'HF=d2/x':>8}")
    for e in (audible[-40:] if not nwin else audible[-nwin:]):
        print(f"{e['seq']:7d} {e['ms']:8d} {e['sample_count']:6d} "
              f"{e['min']:7d} {e['max']:7d} {e['mean_abs']:8d} "
              f"{e['mean_abs_d1']:9d} {e['mean_abs_d2']:9d} {hf(e):8.3f}")

    if aud_ratios:
        sr = sorted(aud_ratios)
        print(f"\n=== HF ratio over {len(audible)} audible chunks ===")
        print(f"min={sr[0]:.3f}  median={sr[len(sr)//2]:.3f}  "
              f"max={sr[-1]:.3f}  mean={sum(sr)/len(sr):.3f}")
        hi = [r for r in aud_ratios if r > 0.5]
        print(f"chunks with HF ratio > 0.5: {len(hi)}/{len(audible)}")
        if sr[len(sr)//2] > 0.5:
            print("!! median HF ratio high — synthesized PCM is jagged/noisy "
                  "(static is baked into the samples, NOT host-side)")
        else:
            print("   median HF ratio low — synthesized PCM is smooth; "
                  "static (if any) is not high-frequency sample noise here")

    if nwin:
        print(f"\n=== raw one-channel windows (last {nwin} audible chunks) ===")
        for e in audible[-nwin:]:
            print(f"seq={e['seq']} mean|x|={e['mean_abs']}: {e['window']}")


if __name__ == "__main__":
    main()
