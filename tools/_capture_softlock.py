"""Multi-command capture for the current softlock. Pulls
post_mortem JSON, recent SP tasks, recent DLs, recent OS messages,
thread dumps, and tail of the error log."""
import socket, json

def ask(cmd):
    s = socket.socket(); s.settimeout(20); s.connect(('127.0.0.1',4371))
    s.sendall((json.dumps(cmd)+'\n').encode())
    buf = b''
    while True:
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            break
        if not chunk: break
        buf += chunk
        if b'\n' in buf and len(buf) > 0 and buf.rstrip().endswith(b'}'):
            break
        if len(buf) > 4_000_000: break
    return buf.decode(errors='replace')

OUT = 'F:/Projects/n64recomp/PokemonStadiumRecomp/build'

print('=== post_mortem_dump ===')
r = ask({'cmd':'post_mortem_dump','path':f'{OUT}/_2026-05-10_register_softlock_pm.json'})
print(r[:600])
print()

print('=== sp_task_recent (16) ===')
r = ask({'cmd':'sp_task_recent','count':16})
print(r[:2500])
print()

print('=== mesg_recent (40) ===')
r = ask({'cmd':'mesg_recent','count':40})
print(r[:2500])
print()

print('=== libultra_recent (40) ===')
r = ask({'cmd':'libultra_recent','count':40})
print(r[:2500])
print()

print('=== dump_threads ===')
r = ask({'cmd':'dump_threads'})
print(r[:3000])
print()

print('=== tail_errlog (60 lines) ===')
r = ask({'cmd':'tail_errlog','lines':60})
print(r[:3000])
