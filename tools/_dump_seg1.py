"""Dump the actual contents at gSegments[1].vaddr (= 0x80206AA0) and see
both: the 80 bytes that ARE the segment, and a wider range showing what
audio data is past the boundary.
"""
import json, socket

def peek(vaddr, n):
    s = socket.create_connection(("127.0.0.1", 4371), timeout=5)
    s.sendall((json.dumps({"cmd": "rdram_peek", "addr": vaddr, "n": n}) + "\n").encode())
    buf = b""
    while True:
        c = s.recv(65536)
        if not c: break
        buf += c
        if buf.endswith(b"\n"): break
    s.close()
    return json.loads(buf.decode())


def hex_words(h):
    return [h[i:i+8] for i in range(0, len(h), 8)]


print("=== gSegments[1].vaddr = 0x80206AA0, size = 0x50 (80 bytes) ===")
print()
# First 80 bytes (the actual segment)
print("--- Inside segment (0x80206AA0..0x80206AF0) ---")
for off in range(0, 0x50, 16):
    r = peek(0x80206AA0 + off, 16)
    if r.get("ok"):
        print(f"  0x{0x80206AA0 + off:08X}: {' '.join(hex_words(r['hex']))}")

print()
print("--- Past segment end, where shadow renderer is reading (0x80206AF0..0x80209000) ---")
# Sample at 0x100 intervals to show the structure
for off in range(0, 0x2A00, 0x100):
    addr = 0x80206AA0 + off
    r = peek(addr, 16)
    if r.get("ok"):
        words = hex_words(r["hex"])
        # Detect content type
        w0 = int(words[0], 16)
        op = (w0 >> 24) & 0xFF
        note = ""
        if op == 0x46:  # 'F' from FRAGMENT
            note = "  text-like"
        elif (w0 & 0xFF000000) == 0x80000000 or (w0 & 0xFF000000) == 0x00000000:
            note = "  pointers-like"
        elif w0 in (0xDB060000, 0xE7000000, 0xE3000F00, 0xDF000000):
            note = "  Gfx-cmd-like"
        elif (w0 >> 16) > 0x7000 and (w0 >> 16) < 0xFFFF:
            note = "  audio-sample-like"
        marker_seg_end = "  <-- past 0x50 segment size" if off >= 0x50 else ""
        marker_target = "  <<< BAD G_DL TARGET" if 0x80206AA0 + off == 0x80208F20 else ""
        print(f"  +0x{off:04X} (0x{addr:08X}): {' '.join(words)}{note}{marker_seg_end}{marker_target}")
