#!/usr/bin/env python3
# Reader for the archive-load + fragment-relocate ring (debug-server cmds
# `arcload_ring` and `arcload_badfrag`), captured via game.toml entry hooks
# on func_8000484C (BinArchive consume) and func_800043BC (relocate
# dispatch). See extras.c arcload_ev / pkmnstadium_relocfrag_enter.
#
# Built to root-cause the menu cursor/icon corruption: the OOB
# Memmap_SetFragmentMap(id=-15) that clobbers gSegments[1] comes from
# func_800043BC(archive->unk_02, frag) with a corrupt/sign-extended id.
# This shows the archive header (unk_00 flags, unk_02 id, num_files,
# compressed source unk_04) at consume time, and the id actually passed
# to the relocator, so we can tell whether the stored byte is wrong, the
# value is sign-extended, or the wrong (in-place/cached) branch was taken.
#
# Usage:
#   python tools/_arcload_ring.py [count]            # timeline (both kinds)
#   python tools/_arcload_ring.py [count] --bad      # only out-of-range ids
#   python tools/_arcload_ring.py --badfrag          # the sticky smoking gun
import socket, json, sys


HOST = ('127.0.0.1', 4371)


def fetch(payload):
    s = socket.create_connection(HOST, timeout=8)
    s.sendall((json.dumps(payload) + '\n').encode())
    buf = b''
    while not buf.endswith(b'\n'):
        c = s.recv(65536)
        if not c:
            break
        buf += c
    s.close()
    return json.loads(buf.decode().strip())


def s32(u):
    return u - 0x100000000 if u >= 0x80000000 else u


def ascii4(u):
    return ''.join(chr(b) if 32 <= b < 127 else '.'
                   for b in (u >> 24 & 0xFF, u >> 16 & 0xFF, u >> 8 & 0xFF, u & 0xFF))


def show_badfrag():
    r = fetch({'cmd': 'arcload_badfrag'})
    if not r.get('ok'):
        print('ERR', r)
        return
    if not r.get('captured'):
        print('no bad-fragment relocate captured yet '
              '(navigate to the repro screen first)')
        return
    rc = r['reloc']
    ar = r['archive']
    idu = rc['id']
    print('=== SMOKING GUN: out-of-range fragment relocate ===')
    print(f"  relocate: id={idu:#010x} ({s32(idu)})  "
          f"frag=0x{rc['frag']:08x}  magic='{ascii4(rc['magic0'])}{ascii4(rc['magic1'])}'  "
          f"relocOffset=0x{rc['relocOffset']:x}  sizeInRam=0x{rc['sizeInRam']:x}")
    print(f"            caller=0x{rc['caller']:08x}  gs=0x{rc['gs']:04x}")
    hdr = ar['unk00unk02']
    print(f"  archive : vaddr=0x{ar['vaddr']:08x}  unk_00=0x{hdr >> 16 & 0xFFFF:04x} (flags)  "
          f"unk_02=0x{hdr & 0xFFFF:04x} (id)  num_files={ar['num_files']}")
    print(f"            unk_04(src)=0x{ar['unk04']:08x}  total_size=0x{ar['total_size']:x}")
    print(f"            file_number={s32(ar['file_number'])}  file.offset=0x{ar['file_offset']:x}  "
          f"file.size=0x{ar['file_size']:x}  file.unk_08(cache)=0x{ar['file_unk08']:08x}")
    print(f"            caller=0x{ar['caller']:08x}")
    flags = hdr >> 16 & 0xFFFF
    print('  branch  : ' + (
        'in-place (unk_00&1) — relocate should NOT run' if flags & 1 else
        'decompress+relocate (unk_00&1 clear)'))


def main():
    args = sys.argv[1:]
    if '--badfrag' in args:
        show_badfrag()
        return
    bad_only = '--bad' in args
    pos = [a for a in args if not a.startswith('--')]
    count = int(pos[0]) if pos else 512

    r = fetch({'cmd': 'arcload_ring', 'count': count})
    if not r.get('ok'):
        print('ERR', r)
        return
    entries = r.get('entries', [])
    print(f"write_idx={r.get('write_idx')} capacity={r.get('capacity')} "
          f"got={len(entries)}")
    for e in entries:
        if e['kind'] == 0:
            flags = e['h0'] >> 16 & 0xFFFF
            uid = e['h0'] & 0xFFFF
            if bad_only:
                continue
            print(f"  seq={e['seq']:6d} ALOAD    archive=0x{e['a']:08x} "
                  f"file={s32(e['b'])} unk_00=0x{flags:04x} unk_02=0x{uid:04x} "
                  f"nfiles={e['h3']} unk_04=0x{e['h1']:08x} "
                  f"f.off=0x{e['e0']:x} f.sz=0x{e['e1']:x} f.cache=0x{e['e2']:08x} "
                  f"gs=0x{e['gs']:04x}")
        else:
            idu = e['a']
            oob = (idu >= 240)
            if bad_only and not oob:
                continue
            mark = '  <<< OOB' if oob else ''
            print(f"  seq={e['seq']:6d} RELOC    id={idu:#010x}({s32(idu)}) "
                  f"frag=0x{e['b']:08x} magic='{ascii4(e['h0'])}{ascii4(e['h1'])}' "
                  f"relocOff=0x{e['h2']:x} sizeInRam=0x{e['h3']:x} "
                  f"caller=0x{e['caller']:08x} gs=0x{e['gs']:04x}{mark}")


if __name__ == '__main__':
    main()
