#!/usr/bin/env python3
"""psr_dbg.py — tiny client for PokemonStadiumRecomp's TCP debug server.

The runner starts a JSON-line TCP debug server (src/main/debug_server.cpp) on
PSR_DEBUG_PORT (default 4371). Each request is one JSON object + '\n'; the reply
is one JSON line.

Usage:
  python tools/psr_dbg.py [--port N] <cmd> [k=v ...]
  python tools/psr_dbg.py status
  python tools/psr_dbg.py ping
  python tools/psr_dbg.py rdram_peek addr=0x80200088 n=64        # hex dump (BE word view)
  python tools/psr_dbg.py deref 0x80200088 0xC 0x28              # pointer-chase: read u32 at base, +off, ...
  python tools/psr_dbg.py trace [n=80]                           # last N guest function entries (trace_recent)
  python tools/psr_dbg.py press CDown [frames=2]                 # down, wait, up  (uses input override)
  python tools/psr_dbg.py raw '{"cmd":"...","k":v}'

Numeric args accept 0x.. hex or decimal.
"""
import socket, sys, json, time

def _num(s):
    s = str(s)
    return int(s, 16) if s.lower().startswith("0x") else int(s, 0)

class Dbg:
    def __init__(self, port, host="127.0.0.1"):
        self.s = socket.create_connection((host, port), timeout=5)
        self.f = self.s.makefile("rwb")
    def cmd(self, **kw):
        self.f.write((json.dumps(kw) + "\n").encode())
        self.f.flush()
        line = self.f.readline()
        try:
            return json.loads(line.decode())
        except Exception:
            return {"raw": line.decode(errors="replace")}
    def close(self):
        try: self.s.close()
        except Exception: pass

def peek_u32(d, vaddr):
    r = d.cmd(cmd="rdram_peek", addr=_num(vaddr) & 0xFFFFFFFF, n=4)
    if not r.get("ok"): return None, r
    return int(r["hex"], 16), r

def main():
    args = sys.argv[1:]
    port = 4371
    if args and args[0] == "--port":
        port = int(args[1]); args = args[2:]
    if not args:
        print(__doc__); return
    d = Dbg(port)
    try:
        cmd = args[0]; rest = args[1:]
        if cmd == "deref":
            # deref BASE OFF1 OFF2 ... : read u32 at BASE, then at (val+OFFi) each step
            base = _num(rest[0]); offs = [_num(x) for x in rest[1:]]
            cur = base; chain = [f"base=0x{base:08X}"]
            val, r = peek_u32(d, cur)
            if val is None: print("peek fail", r); return
            chain.append(f"[0x{cur:08X}]=0x{val:08X}")
            for off in offs:
                cur = (val + off) & 0xFFFFFFFF
                val, r = peek_u32(d, cur)
                if val is None:
                    chain.append(f"+0x{off:X} -> [0x{cur:08X}] FAIL {r}"); break
                chain.append(f"+0x{off:X} -> [0x{cur:08X}]=0x{val:08X}")
            print("  ".join(chain))
        elif cmd == "press":
            name = rest[0]
            frames = 2
            for kv in rest[1:]:
                if kv.startswith("frames="): frames = int(kv.split("=",1)[1])
            print(d.cmd(cmd="set_button", name=name, down=True))
            time.sleep(max(0.05, frames/60.0))
            print(d.cmd(cmd="set_button", name=name, down=False))
        elif cmd == "trace":
            n = 80
            for kv in rest:
                if kv.startswith("n="): n = int(kv.split("=",1)[1])
            print(json.dumps(d.cmd(cmd="trace_recent", n=n), indent=None))
        elif cmd == "raw":
            d.f.write((rest[0] + "\n").encode()); d.f.flush()
            print(d.f.readline().decode(errors="replace").rstrip())
        else:
            kw = {"cmd": cmd}
            for kv in rest:
                k, v = kv.split("=", 1)
                try: kw[k] = _num(v)
                except ValueError: kw[k] = v
            print(json.dumps(d.cmd(**kw)))
    finally:
        d.close()

if __name__ == "__main__":
    main()
