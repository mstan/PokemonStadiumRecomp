"""Drive a fresh boot to GB Tower and optionally launch a cart.

This is a diagnostic harness for the TCP debug server. It records
screenshots at the known stable menu states so route regressions and
post-launch crashes have reproducible evidence.
"""
from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

ROOT = Path("F:/Projects/n64recomp/PokemonStadiumRecomp")
RUNNER = ROOT / "build" / "PokemonStadiumRecomp.exe"
SCREENSHOT = ROOT / "tools" / "_screenshot.ps1"
ERRLOG = ROOT / "build" / "last_error.log"
HOST = ("127.0.0.1", int(os.environ.get("PSR_DEBUG_PORT", "4371")))


def call(cmd: dict[str, Any], timeout: float = 4.0) -> dict[str, Any]:
    try:
        sock = socket.create_connection(HOST, timeout=timeout)
    except OSError as exc:
        return {"_err": f"connect: {exc}"}
    try:
        sock.sendall((json.dumps(cmd) + "\n").encode())
        sock.settimeout(timeout)
        data = b""
        while True:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                break
            except OSError as exc:
                return {"_err": f"recv: {exc}"}
            if not chunk:
                break
            data += chunk
            if data.endswith(b"\n"):
                break
    finally:
        sock.close()
    text = data.decode(errors="replace").strip()
    if not text:
        return {"_err": "empty response"}
    try:
        return json.loads(text)
    except Exception as exc:
        return {"_err": f"json: {exc}", "_raw": text}


def u32_be(hex_text: str, offset: int = 0) -> int | None:
    if len(hex_text) < offset * 2 + 8:
        return None
    raw = bytes.fromhex(hex_text[offset * 2 : offset * 2 + 8])
    return int.from_bytes(raw, "big")


def frag_addr(fragment_id: int, link_addr: int) -> int | None:
    frag = call({"cmd": "frag_section", "id": fragment_id}, timeout=4.0)
    if not frag.get("registered"):
        return None
    frag_base = int(frag.get("section_addr", 0))
    frag_link = int(frag.get("link_addr", 0))
    if frag_base == 0 or frag_link == 0:
        return None
    return frag_base + (link_addr - frag_link)


def wait_ping(proc: subprocess.Popen[Any] | None, timeout_s: float = 30.0) -> None:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if proc is not None and proc.poll() is not None:
            raise RuntimeError(f"runner exited before TCP server: {proc.returncode}")
        if call({"cmd": "ping"}, timeout=1.0).get("ok"):
            return
        time.sleep(0.5)
    raise RuntimeError("TCP server never answered ping")


def press(name: str, hold_s: float = 0.16) -> None:
    call({"cmd": "set_button", "name": name, "down": True}, timeout=2.0)
    time.sleep(hold_s)
    call({"cmd": "set_button", "name": name, "down": False}, timeout=2.0)


def capture(path: Path, proc: subprocess.Popen[Any] | None = None) -> None:
    cmd = [
        "powershell",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(SCREENSHOT),
        str(path),
    ]
    if proc is not None:
        cmd.extend(["-ProcessId", str(proc.pid)])
    subprocess.run(
        cmd,
        cwd=str(ROOT),
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=15,
        check=False,
    )


def collect_peeks() -> dict[str, Any]:
    peeks: dict[str, Any] = {}

    frag2 = call({"cmd": "frag_section", "id": 2}, timeout=4.0)
    peeks["frag2"] = frag2
    frag2_base = int(frag2.get("section_addr", 0)) if frag2.get("registered") else 0
    frag2_link = int(frag2.get("link_addr", 0)) if frag2.get("registered") else 0

    def frag2_addr(link_addr: int) -> int | None:
        if not frag2_base or not frag2_link:
            return None
        return frag2_base + (link_addr - frag2_link)

    for name, addr, n in [
        ("scheduler_800A6CC0", 0x800A6CC0, 0xA0),
        ("mq_vi_800A6CD0", 0x800A6CD0, 0x40),
        ("mq_sched_800A8640", 0x800A8640, 0x40),
        ("mq_dp_80083E60", 0x80083E60, 0x40),
        ("mq_gb_main_80119900", 0x80119900, 0x40),
        ("mq_gb_audio_8011BB20", 0x8011BB20, 0x40),
    ]:
        peeks[name] = call({"cmd": "rdram_peek", "addr": addr, "n": n}, timeout=4.0)

    for name, link_addr, n in [
        ("gb_state_8120EA80", 0x8120EA80, 0x100),
        ("gb_type_flags_8120D90C", 0x8120D90C, 0x80),
        ("gb_ptr_global_8122B2C0", 0x8122B2C0, 0x80),
        ("gb_state_8122C4D0", 0x8122C4D0, 0x80),
        ("gb_audio_81234690", 0x81234690, 0x80),
    ]:
        addr = frag2_addr(link_addr)
        if addr is None:
            peeks[name] = {"ok": False, "error": "fragment 2 not registered"}
        else:
            peeks[name] = call({"cmd": "rdram_peek", "addr": addr, "n": n}, timeout=4.0)

    ptr_hex = peeks.get("gb_ptr_global_8122B2C0", {}).get("hex")
    gb_ptr = u32_be(ptr_hex) if isinstance(ptr_hex, str) else None
    peeks["gb_struct_ptr"] = gb_ptr
    if gb_ptr is not None and gb_ptr != 0:
        for off in [
            0x0000,
            0x0100,
            0x0140,
            0x0180,
            0x0200,
            0x0300,
            0x0400,
            0x5300,
            0x5380,
            0x5400,
            0x5480,
            0x5500,
            0x5580,
            0x5600,
            0x5C00,
            0x5C80,
            0x5D00,
            0x5D80,
        ]:
            peeks[f"gb_struct_{off:04X}"] = call(
                {"cmd": "rdram_peek", "addr": gb_ptr + off, "n": 0x100},
                timeout=4.0,
            )
    return peeks


def collect_gb_runtime_pointer_peeks(report: dict[str, Any]) -> dict[str, Any]:
    peeks: dict[str, Any] = {}
    events = report.get("gbtower_trace", {}).get("events", [])
    if not isinstance(events, list) or not events:
        return peeks
    last = events[-1]
    if not isinstance(last, dict):
        return peeks

    words = last.get("words")
    cpu = last.get("cpu")
    if isinstance(words, list) and len(words) > 5 and isinstance(words[5], int):
        header_base = words[5]
        peeks["rom_header_at_w53bc"] = {
            "addr": header_base,
            "peek": call({"cmd": "rdram_peek", "addr": header_base, "n": 0x180}, timeout=4.0),
        }
    if isinstance(cpu, dict):
        for label in ("pc_base", "pc_addr"):
            value = cpu.get(label)
            if isinstance(value, int) and value != 0:
                peeks[label] = {
                    "addr": value,
                    "peek": call({"cmd": "rdram_peek", "addr": value, "n": 0x100}, timeout=4.0),
                }
    return peeks


def collect_vi_framebuffer_peeks(vi_state: dict[str, Any]) -> dict[str, Any]:
    peeks: dict[str, Any] = {}
    for key in ("origin", "cur_fb", "next_fb"):
        raw = vi_state.get(key)
        if raw is None:
            continue
        addr = int(raw) & 0x1FFFFFFF
        peeks[key] = {
            "digest": call({"cmd": "rdram_digest", "addr": addr, "n": 0x25800}, timeout=4.0),
            "head": call({"cmd": "rdram_peek", "addr": addr, "n": 0x100}, timeout=4.0),
        }
    return peeks


def collect_sp_task_buffer_digests(sp_task_recent: dict[str, Any]) -> dict[str, Any]:
    peeks: dict[str, Any] = {}
    seen: set[tuple[str, int]] = set()

    def add(label: str, addr: int | None, n: int) -> None:
        if addr is None or addr == 0:
            return
        paddr = int(addr) & 0x1FFFFFFF
        if paddr >= 8 * 1024 * 1024:
            return
        key = (label, paddr)
        if key in seen:
            return
        seen.add(key)
        peeks[f"{label}_{paddr:06X}"] = {
            "addr": paddr,
            "digest": call({"cmd": "rdram_digest", "addr": paddr, "n": n}, timeout=4.0),
            "head": call({"cmd": "rdram_peek", "addr": paddr, "n": 0x100}, timeout=4.0),
        }

    events = sp_task_recent.get("events", [])
    if not isinstance(events, list):
        return peeks

    for ev in events[-16:]:
        if not isinstance(ev, dict):
            continue
        add("output_buff", ev.get("output_buff"), 0x25800)
        add("output_scratch", ev.get("output_buff_size"), 0x6000)
        add("dram_stack", (ev.get("task_words") or [None] * 16)[8] if isinstance(ev.get("task_words"), list) else None, 0x6000)
        add("data_ptr", ev.get("data_ptr"), 0x540)
        task_words = ev.get("task_words")
        if isinstance(task_words, list) and len(task_words) >= 16:
            add("ucode_boot", task_words[2], min(int(task_words[3]) if task_words[3] else 0xA40, 0x2000))
            add("ucode_data", task_words[6], min(int(task_words[7]) if task_words[7] else 0xFC0, 0x2000))
            add("yield_data", task_words[14], 0x1000)
            ucode_data = task_words[6]
            if isinstance(ucode_data, int):
                add("ucode_data_f80", ucode_data + 0xF80, 0x80)
    return peeks


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cart", choices=["yellow", "red", "blue"], default="red")
    parser.add_argument("--out", default=str(ROOT / "build" / "gbtower_route_latest"))
    parser.add_argument("--launch-cart", action="store_true")
    parser.add_argument("--no-launch", action="store_true",
                        help="Drive an already-running runner/debugger over TCP.")
    parser.add_argument("--post-action", action="append", default=[],
                        help="After cart launch: wait:seconds, press:Button[:hold], or capture:label.")
    parser.add_argument("--post-launch-wait", type=float, default=5.0)
    args = parser.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    try:
        ERRLOG.write_text("")
    except OSError:
        pass

    stdout_file = None
    stderr_file = None
    proc: subprocess.Popen[Any] | None = None
    if not args.no_launch:
        stdout_file = (out / "runner.stdout.log").open("w", encoding="utf-8", errors="replace")
        stderr_file = (out / "runner.stderr.log").open("w", encoding="utf-8", errors="replace")
        proc = subprocess.Popen(
            [str(RUNNER)],
            cwd=str(RUNNER.parent),
            stdin=subprocess.DEVNULL,
            stdout=stdout_file,
            stderr=stderr_file,
        )

    report: dict[str, Any] = {"cart": args.cart, "events": []}
    t0 = time.time()

    def event(kind: str, **data: Any) -> None:
        rec = {"t": round(time.time() - t0, 3), "kind": kind, **data}
        report["events"].append(rec)
        print(f"[gbtower] {kind}: {json.dumps(data, default=str)}")

    try:
        wait_ping(proc)
        event("connected")
        event("claim_input", **call({"cmd": "claim_input"}, timeout=2.0))

        route = [
            ("01_after_first_start", "Start", 4.2),
            ("02_title", "Start", 4.2),
            ("03_gamepak_check", "A", 4.2),
            ("04_main_select", "A", 4.2),
            ("05_stadium_map", "A", 4.2),
            ("06_gbtower_highlighted", "DRight", 1.5),
            ("07_cart_select", "A", 2.5),
        ]
        for label, button, wait_s in route:
            if proc is not None and proc.poll() is not None:
                event("runner_exited", returncode=proc.returncode, before=label)
                break
            press(button)
            event("press", label=label, button=button)
            time.sleep(wait_s)
            capture(out / f"{label}.png", proc)
            event("capture", path=str(out / f"{label}.png"))

        cart_moves = {"yellow": 0, "red": 1, "blue": 2}[args.cart]
        for i in range(cart_moves):
            press("DRight")
            event("press", label=f"cart_move_{i + 1}", button="DRight")
            time.sleep(1.0)
        capture(out / f"08_{args.cart}_selected.png", proc)
        event("capture", path=str(out / f"08_{args.cart}_selected.png"))

        if args.launch_cart and (proc is None or proc.poll() is None):
            press("A")
            event("press", label="launch_cart", button="A")
            time.sleep(args.post_launch_wait)
            if proc is None:
                if call({"cmd": "ping"}, timeout=1.0).get("ok"):
                    capture(out / f"09_after_{args.cart}_launch.png", proc)
                    event("capture", path=str(out / f"09_after_{args.cart}_launch.png"))
                else:
                    event("runner_unreachable", after="launch_cart")
            elif proc.poll() is None:
                capture(out / f"09_after_{args.cart}_launch.png", proc)
                event("capture", path=str(out / f"09_after_{args.cart}_launch.png"))
            else:
                event("runner_exited", returncode=proc.returncode, after="launch_cart")

            for action in args.post_action:
                if proc is not None and proc.poll() is not None:
                    event("runner_exited", returncode=proc.returncode, before_action=action)
                    break
                kind, _, rest = action.partition(":")
                if kind == "wait":
                    delay = float(rest)
                    time.sleep(delay)
                    event("wait", seconds=delay)
                elif kind == "press":
                    button, _, hold = rest.partition(":")
                    press(button, float(hold) if hold else 0.16)
                    event("press", label="post_action", button=button)
                elif kind == "capture":
                    path = out / f"{rest}.png"
                    capture(path, proc)
                    event("capture", path=str(path))
                elif kind in ("poke_gb8", "poke_gb16", "poke_gb32"):
                    off_s, _, val_s = rest.partition(":")
                    gb_global = frag_addr(2, 0x8122B2C0)
                    if gb_global is None:
                        event("post_action_error", action=action, error="fragment 2 unavailable")
                        continue
                    gb_base_hex = call({"cmd": "rdram_peek", "addr": gb_global, "n": 4}, timeout=4.0).get("hex")
                    gb_base = u32_be(gb_base_hex) if isinstance(gb_base_hex, str) else None
                    if gb_base is None or gb_base == 0:
                        event("post_action_error", action=action, error="gb struct pointer unavailable")
                        continue
                    if val_s.startswith("link:"):
                        link_addr = int(val_s[5:], 0)
                        value = frag_addr(2, link_addr)
                    else:
                        value = int(val_s, 0)
                    if value is None:
                        event("post_action_error", action=action, error="value unavailable")
                        continue
                    size = {"poke_gb8": 1, "poke_gb16": 2, "poke_gb32": 4}[kind]
                    addr = gb_base + int(off_s, 0)
                    res = call({"cmd": "rdram_poke", "addr": addr, "hex": int(value).to_bytes(size, "big").hex()}, timeout=4.0)
                    event(kind, offset=off_s, addr=addr, value=int(value), result=res)
                elif kind in ("poke_link8", "poke_link16", "poke_link32"):
                    link_s, _, val_s = rest.partition(":")
                    addr = frag_addr(2, int(link_s, 0))
                    if addr is None:
                        event("post_action_error", action=action, error="fragment 2 unavailable")
                        continue
                    if val_s.startswith("link:"):
                        value = frag_addr(2, int(val_s[5:], 0))
                    else:
                        value = int(val_s, 0)
                    if value is None:
                        event("post_action_error", action=action, error="value unavailable")
                        continue
                    size = {"poke_link8": 1, "poke_link16": 2, "poke_link32": 4}[kind]
                    res = call({"cmd": "rdram_poke", "addr": addr, "hex": int(value).to_bytes(size, "big").hex()}, timeout=4.0)
                    event(kind, link=link_s, addr=addr, value=int(value), result=res)
                else:
                    event("post_action_error", action=action, error="unknown action")

        report["dump_threads"] = call({"cmd": "dump_threads"}, timeout=4.0)
        report["status"] = call({"cmd": "status"}, timeout=2.0)
        report["trace_recent"] = call({"cmd": "trace_recent", "n": 128}, timeout=4.0)
        report["interesting_fns"] = call({"cmd": "interesting_fns"}, timeout=4.0)
        report["gbtower_trace"] = call({"cmd": "gbtower_trace", "n": 32768}, timeout=8.0)
        report["gb_runtime_pointer_peeks"] = collect_gb_runtime_pointer_peeks(report)
        report["vi_state"] = call({"cmd": "vi_state"}, timeout=4.0)
        report["vi_framebuffer_peeks"] = collect_vi_framebuffer_peeks(report["vi_state"])
        report["libultra_recent"] = call({"cmd": "libultra_recent", "n": 128}, timeout=4.0)
        report["mesg_recent"] = call({"cmd": "mesg_recent", "n": 256}, timeout=4.0)
        report["sp_task_recent"] = call({"cmd": "sp_task_recent", "n": 64}, timeout=4.0)
        report["sp_task_buffer_digests"] = collect_sp_task_buffer_digests(report["sp_task_recent"])
        report["rsp_dma_recent"] = call({"cmd": "rsp_dma_recent", "n": 1024}, timeout=4.0)
        report["rsp_pc_trail"] = call({"cmd": "get_last_pc_trail"}, timeout=4.0)
        report["gb_state_peeks"] = collect_peeks()
        report["tail_errlog"] = call({"cmd": "tail_errlog"}, timeout=4.0)
        report["runner_returncode"] = proc.poll() if proc is not None else None
    finally:
        if proc is not None and proc.poll() is None:
            call({"cmd": "quit"}, timeout=1.0)
            time.sleep(0.5)
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=5.0)
            except subprocess.TimeoutExpired:
                proc.kill()
        if stdout_file is not None:
            stdout_file.close()
        if stderr_file is not None:
            stderr_file.close()
        report["runner_returncode_final"] = proc.poll() if proc is not None else None
        (out / "route_report.json").write_text(json.dumps(report, indent=2, default=str))
        event("report", path=str(out / "route_report.json"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
