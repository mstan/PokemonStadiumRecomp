"""Read key globals from RDRAM during the softlock."""
import socket, json, struct
HOST, PORT = "127.0.0.1", 4371

def call(cmd, timeout=4):
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    s.sendall((cmd + "\n").encode()); s.settimeout(timeout)
    buf = b""
    try:
        while True:
            c = s.recv(65536)
            if not c: break
            buf += c
            if buf.endswith(b"\n"): break
    except socket.timeout: pass
    s.close()
    try: return json.loads(buf.decode(errors="replace").strip())
    except Exception: return {"_raw": buf.decode(errors="replace").strip()}

def peek_u32(addr):
    r = call(f'{{"cmd":"rdram_peek","addr":{addr},"n":4}}')
    raw = (r.get("hex") or r.get("data") or "").replace(" ", "")
    if not raw: return None, r
    return struct.unpack(">I", bytes.fromhex(raw))[0], r

GAMESTATE_NAMES = {
    0x01: "N64_LOGO_INTRO",
    0x02: "TITLE_SCREEN",
    0x03: "N64DD_BOOT_UNUSED",
    0x04: "AREA_SELECT",
    0x05: "GALLERY",
    0x06: "EVENT_BATTLE",
    0x07: "OPTIONS",
    0x08: "MENU_SELECT",
    0x09: "STADIUM_MENU",
    0x0A: "FREE_BATTLE",
    0x0B: "VS_MEWTWO",
    0x0C: "KIDS_CLUB",
    0x0D: "VICTORY_PALACE",
    0x0E: "POKEMON_LAB",
    0x0F: "GB_TOWER",
    0x10: "GYM_LEADER_CASTLE",
    0x11: "BATTLE_NOW_1P",
    0x12: "BATTLE_NOW_2P",
    0x13: "BATTLE_FROM_EVENT",
    0x14: "STUBBED_DEBUG",
    0x15: "FAST_BATTLE",
    0x16: "KIDS_CLUB_TITLE",
    0x17: "FAST_N64_LOGO",
}

state, _ = peek_u32(0x80075668)
last_state, _ = peek_u32(0x80075668 + 4)  # gLastGameState immediately after
print(f"gCurrentGameState = 0x{state:08x} ({GAMESTATE_NAMES.get(state, '?')})")
print(f"<after gCurrent>   = 0x{last_state:08x}")

# Read main_pool magic/state to see if we're inside a fragment
# Stadium uses 'TITL'/'GAME'/'STAD' main_pool tags. Let's just hexdump a wider region.
r = call('{"cmd":"rdram_peek","addr":2148145664,"n":64}')  # 0x80075600
print()
print("hex around gCurrentGameState (0x80075600..0x8007563f):")
print(r)
