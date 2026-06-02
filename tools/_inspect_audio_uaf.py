"""Inspect the Register Pokemon audio UAF crash.

Goal: triangulate the bad voice descriptor by:
  1. Querying what's at low RDRAM 0x0..0x2000 (where the bad ptr 0x80001007 lands)
  2. Probing live memory at typical voice-list locations from the prior audio UAF notes
  3. Walking pAllocList / pLameList for synthesizer N_ALSynth state
"""
import socket, json

def ask(cmd):
    s = socket.socket(); s.settimeout(20); s.connect(('127.0.0.1',4371))
    s.sendall((json.dumps(cmd)+'\n').encode())
    buf = b''
    while True:
        try: chunk = s.recv(65536)
        except socket.timeout: break
        if not chunk: break
        buf += chunk
        if b'\n' in buf and buf.rstrip().endswith(b'}'):
            break
        if len(buf) > 4_000_000: break
    return json.loads(buf.decode(errors='replace'))

# 1. What's at vaddr 0x80001007? In RDRAM that's offset 0x1007.
print('=== Memory at the deref target (vaddr 0x80001000..0x80001020) ===')
r = ask({'cmd':'rdram_peek','addr':0x80001000,'n':32})
print(f"  hex: {r.get('hex','')}")
print()

print('=== Memory near the bad ptr if it is something else (try 0x80001007 -1 alignment) ===')
r = ask({'cmd':'rdram_peek','addr':0x80000ff0,'n':64})
print(f"  hex: {r.get('hex','')}")
print()

# Audio frame status from libultra ring
print('=== Status (post-crash) ===')
print(json.dumps(ask({'cmd':'status'}), indent=2)[:600])
print()

# Pull the post_mortem report's "trace ring" subset to see what audio funcs ran
print('=== libultra trace tail showing audio context (last 10 entries with *al* in name) ===')
r = ask({'cmd':'libultra_recent','count':200})
events = r.get('events',[])
audio_events = [e for e in events if 'al' in e.get('name','').lower() or 'amDMA' in e.get('name','')]
for e in audio_events[-12:]:
    print(f"  i={e.get('i')} {e.get('name'):30s} pc={e.get('pc'):#x} a0={e.get('a0'):#x} a1={e.get('a1'):#x} a2={e.get('a2'):#x}")
