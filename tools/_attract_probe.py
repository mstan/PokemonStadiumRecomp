"""Attract demo soft-lock probe.

Polls TCP status periodically + reads gCurrentGameState (vaddr 0x80075668)
+ optionally pulls trace_recent / mesg_recent / sp_task_recent rings.

Usage: python tools/_attract_probe.py [--samples N] [--interval SEC] [--mode short|deep]
"""
import argparse, json, socket, sys, time

HOST, PORT = '127.0.0.1', 4371

STATE_NAMES = {
    0x01: "N64_LOGO_INTRO",
    0x02: "TITLE_SCREEN",
    0x03: "N64DD_BOOT_UNUSED",
    0x04: "MENU_SELECT",
    0x10: "AREA_SELECT",
    0x11: "GALLERY",
    0x12: "EVENT_BATTLE",
    0x13: "OPTIONS",
    0x20: "STADIUM_MENU",
    0x21: "FREE_BATTLE",
    0x22: "VS_MEWTWO",
    0x23: "KIDS_CLUB",
    0x24: "VICTORY_PALACE",
    0x25: "POKEMON_LAB",
    0x26: "GB_TOWER",
    0x27: "GYM_LEADER_CASTLE",
    0x28: "BATTLE_NOW_1P",
    0x29: "BATTLE_NOW_2P",
    0x2A: "BATTLE_FROM_EVENT",
    0x40: "FAST_N64_LOGO",
    0x80: "STUBBED_DEBUG",
    0x81: "FAST_BATTLE",
    0x82: "KIDS_CLUB_TITLE",
}


def send(cmd):
    s = socket.create_connection((HOST, PORT), timeout=5)
    s.sendall((json.dumps(cmd) + "\n").encode())
    buf = b''
    while True:
        c = s.recv(65536)
        if not c:
            break
        buf += c
        if buf.endswith(b'\n'):
            break
    s.close()
    return json.loads(buf.decode())


def state_word():
    """Read u32 BE at vaddr 0x80075668 → state byte (LSB)."""
    r = send({"cmd": "rdram_peek", "addr": 0x80075668, "n": 4})
    if not r.get("ok"):
        return None, None
    hexs = r["hex"]  # 8 hex chars, big-endian
    raw = int(hexs, 16)
    state = raw & 0xFF  # LSB
    return raw, state


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--samples", type=int, default=20)
    ap.add_argument("--interval", type=float, default=3.0)
    ap.add_argument("--mode", choices=("short", "deep"), default="short")
    args = ap.parse_args()

    prev = None
    print(f"[attract-probe] {args.samples} samples @ {args.interval}s")
    for i in range(args.samples):
        try:
            st = send({"cmd": "status"})
            raw, state = state_word()
            name = STATE_NAMES.get(state, f"?0x{state:02X}") if state is not None else "?"
            line = (
                f"[{i:02d}] frame={st['frame']:>6} vi={st['vi']:>6} "
                f"gfx={st['send_dl_gfx']:>5} aud={st['send_dl_audio']:>5} "
                f"oth={st['send_dl_other']:>4} sub_gfx={st['submit_gfx']:>5} "
                f"dp={st['dp_complete']:>5} sp={st['sp_complete']:>5} "
                f"state=0x{state:02X}({name}) raw=0x{raw:08X}"
                if state is not None else
                f"[{i:02d}] frame={st['frame']:>6} vi={st['vi']:>6} "
                f"gfx={st['send_dl_gfx']:>5} aud={st['send_dl_audio']:>5} "
                f"sub_gfx={st['submit_gfx']:>5} dp={st['dp_complete']:>5} state=N/A"
            )
            if prev:
                df = st['frame'] - prev['frame']
                dgfx = st['send_dl_gfx'] - prev['send_dl_gfx']
                daud = st['send_dl_audio'] - prev['send_dl_audio']
                line += f" dfr={df:>5} dgfx={dgfx:>4} daud={daud:>4}"
            print(line, flush=True)
            prev = st
        except Exception as e:
            print(f"[{i:02d}] probe failed: {e}", flush=True)
        time.sleep(args.interval)

    if args.mode == "deep":
        print("\n=== TRACE_RECENT (last 64) ===")
        r = send({"cmd": "trace_recent", "n": 64})
        for n in r.get("entries", []):
            print(" ", n)

        print("\n=== SP_TASK_RECENT (last 8 gfx-or-audio) ===")
        r = send({"cmd": "sp_task_recent", "n": 16})
        for ev in r.get("events", []):
            print(f"  seq={ev['seq']} frame={ev['frame']} type={ev['type']} "
                  f"task=0x{ev['task_ptr']:08X} ucode=0x{ev['ucode']:08X} "
                  f"data=0x{ev['data_ptr']:08X} size=0x{ev['data_size']:X}")

        print("\n=== MESG_RECENT (last 32) ===")
        r = send({"cmd": "mesg_recent", "n": 32})
        for ev in r.get("events", []):
            print(f"  seq={ev['seq']} ms={ev['ms']} op={ev['op']} mq={ev['mq']:>10} "
                  f"msg={ev['msg']:>10} vb={ev['vb']} va={ev['va']} gt={ev['gt']}")


if __name__ == "__main__":
    main()
