"""Wait for the attract-demo softlock and capture the in-flight DL.

Loop:
  - poll status (every 1s)
  - if send_dl_gfx hasn't changed for >5s AND submit_audio has, query current_dl
  - if current_dl reports in_flight=true AND elapsed_ms > 5000, this is the hanging DL
  - dump rdram[data_ptr, data_ptr+data_size) to build/softlock_hung_dl.bin
  - print the in-flight state and exit

Result: build/softlock_hung_dl.bin contains the exact bytes of the DL that
RT64's send_dl hung on, plus stderr-printed metadata.
"""
import socket, json, time, sys, os


def call(cmd, t=4):
    try:
        s = socket.create_connection(('127.0.0.1', 4371), timeout=t)
    except OSError:
        return None
    try:
        s.sendall((json.dumps(cmd) + '\n').encode())
        buf = b''
        s.settimeout(t)
        while True:
            try:
                c = s.recv(65536)
            except socket.timeout:
                break
            if not c:
                break
            buf += c
            if buf.endswith(b'\n'):
                break
    finally:
        s.close()
    try:
        return json.loads(buf.decode().strip())
    except Exception:
        return None


def peek(addr, n):
    """Read up to 256 bytes per call; chunk for larger."""
    out = b''
    while n > 0:
        c = min(256, n)
        r = call({'cmd': 'rdram_peek', 'addr': addr, 'n': c})
        if not r or 'hex' not in r:
            return out
        out += bytes.fromhex(r['hex'])
        addr += c
        n -= c
    return out


def main():
    print('watching for stall (send_dl_gfx frozen >5s, audio climbing)')
    last_gfx, last_audio, last_change = None, None, time.time()
    while True:
        st = call({'cmd': 'status'})
        if not st or not st.get('ok'):
            time.sleep(1)
            continue
        g = st['send_dl_gfx']
        a = st['submit_audio']

        if last_gfx is None:
            last_gfx, last_audio, last_change = g, a, time.time()
        elif g != last_gfx:
            last_gfx, last_change = g, time.time()

        stalled = time.time() - last_change
        audio_moving = a > last_audio
        last_audio = a

        if stalled > 5 and audio_moving:
            print(f'[STALL] gfx frozen {stalled:.1f}s — send_dl_gfx={g}, dp_complete={st["dp_complete"]}, frame={st["frame"]}')
            dl = call({'cmd': 'current_dl'})
            print(f'current_dl: {dl}')

            if dl and dl.get('in_flight') and dl['data_size'] > 0:
                addr = dl['data_ptr']
                size = dl['data_size']
                if size > 1024*1024:
                    print(f'  data_size={size} too large; capping at 1 MiB')
                    size = 1024*1024
                print(f'  capturing {size} bytes from 0x{addr:08X}...')
                data = peek(addr, size)
                outp = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/softlock_hung_dl.bin'
                with open(outp, 'wb') as f:
                    f.write(data)
                print(f'  saved {len(data)} bytes -> {outp}')

                # Capture context: pre + post 128 bytes too
                pre = peek(addr - 128, 128) if addr > 0x80000080 else b''
                post = peek(addr + size, 128)
                outp2 = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/softlock_hung_dl_context.bin'
                with open(outp2, 'wb') as f:
                    f.write(pre + b'\x00\x00\x00\x00' + data[:64] + b'\x00\x00\x00\x00' + post)
                print(f'  context (128B pre + 64B head + 128B post) -> {outp2}')

                # Also dump status snapshot
                outp3 = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build/softlock_status.json'
                with open(outp3, 'w') as f:
                    json.dump({'status': st, 'current_dl': dl}, f, indent=2)
                print(f'  status snapshot -> {outp3}')

            # Trigger the full post-mortem dump — captures RT64 interpreter
            # probe state (current_pc, step count, recent_pcs[], cf_events[])
            # to build/last_run_report.json. That tells us EXACTLY where in
            # the DL processing RT64 is stuck.
            print('  triggering post_mortem_dump (captures RT64 interp probe)...')
            pm = call({'cmd': 'post_mortem_dump'})
            print(f'  post_mortem: {pm}')
            return 0

        time.sleep(1)


if __name__ == '__main__':
    sys.exit(main() or 0)
