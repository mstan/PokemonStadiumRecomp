#!/usr/bin/env python3
"""N64 co-sim coordinator for Pokemon Stadium Recomp.

Tier-0 focus: Gate 1 A-vs-A determinism. Launch two recomp instances with
N64_COSIM enabled, drive deterministic VI checkpoints through the TCP debug
server, and compare full-state checkpoint hashes at every boundary.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import time
from typing import Any


def project_root() -> Path:
    for candidate in (Path.cwd(), Path(__file__).resolve().parents[1]):
        if (candidate / "CMakeLists.txt").exists() and (candidate / "src" / "main").exists():
            return candidate
    return Path(__file__).resolve().parents[1]


ROOT = project_root()
BUILD = ROOT / "build"
DEFAULT_EXE = BUILD / "PokemonStadiumRecomp.exe"


class CosimError(RuntimeError):
    pass


def host_path(value: str | Path) -> Path:
    path = Path(value)
    if not path.is_absolute():
        path = Path.cwd() / path
    text = str(path).replace("\\", "/")
    if os.name == "nt" and len(text) >= 3 and text[0] == "/" and text[1].isalpha() and text[2] == "/":
        return Path(f"{text[1].upper()}:{text[2:]}")
    return Path(text).resolve()


class DebugClient:
    def __init__(self, host: str, port: int) -> None:
        self.host = host
        self.port = port
        self.sock: socket.socket | None = None
        self.file = None

    def connect(self, timeout_s: float) -> None:
        deadline = time.monotonic() + timeout_s
        last_error: OSError | None = None
        while time.monotonic() < deadline:
            try:
                self.sock = socket.create_connection((self.host, self.port), timeout=2.0)
                self.file = self.sock.makefile("r", encoding="utf-8", newline="\n")
                return
            except OSError as exc:
                last_error = exc
                time.sleep(0.1)
        raise CosimError(f"tcp connect {self.host}:{self.port} failed: {last_error}")

    def cmd(self, payload: str | dict[str, Any], timeout_s: float = 10.0) -> dict[str, Any]:
        if self.sock is None or self.file is None:
            raise CosimError("debug client is not connected")
        line = json.dumps(payload, separators=(",", ":")) if isinstance(payload, dict) else payload
        self.sock.settimeout(timeout_s)
        self.sock.sendall((line + "\n").encode("utf-8"))
        reply = self.file.readline()
        if reply == "":
            raise CosimError(f"debug server {self.port} closed connection after {line!r}")
        try:
            obj = json.loads(reply)
        except json.JSONDecodeError as exc:
            raise CosimError(f"invalid json from {self.port}: {reply!r}") from exc
        return obj

    def close(self) -> None:
        try:
            if self.sock is not None:
                try:
                    self.sock.sendall(b"quit\n")
                except OSError:
                    pass
                self.sock.close()
        finally:
            self.sock = None
            self.file = None


class Instance:
    def __init__(self, name: str, exe: Path, cwd: Path, port: int, log_dir: Path) -> None:
        self.name = name
        self.exe = exe
        self.cwd = cwd
        self.port = port
        self.log_dir = log_dir
        self.proc: subprocess.Popen[bytes] | None = None
        self.client = DebugClient("127.0.0.1", port)
        self.stdout_file = None
        self.stderr_file = None

    def start(self) -> None:
        env = os.environ.copy()
        env["PSR_DEBUG_PORT"] = str(self.port)
        env["PSR_AUTOBOOT"] = "1"
        env["PSR_COSIM_WAIT_START"] = "1"
        env.setdefault("PSR_TURBO", "0")
        env.pop("PSR_COSIM_ENABLE_VOLUNTARY_PREEMPTION", None)

        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.stdout_file = (self.log_dir / f"{self.name}.stdout.log").open("wb")
        self.stderr_file = (self.log_dir / f"{self.name}.stderr.log").open("wb")
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self.proc = subprocess.Popen(
            [str(self.exe)],
            cwd=str(self.cwd),
            env=env,
            stdin=subprocess.DEVNULL,
            stdout=self.stdout_file,
            stderr=self.stderr_file,
            creationflags=creationflags,
        )

    def connect(self, timeout_s: float) -> None:
        self.client.connect(timeout_s)

    def cmd(self, payload: str | dict[str, Any], timeout_s: float = 10.0) -> dict[str, Any]:
        if self.proc is not None and self.proc.poll() is not None:
            raise CosimError(f"{self.name} exited with rc={self.proc.returncode}")
        return self.client.cmd(payload, timeout_s)

    def stop(self) -> None:
        self.client.close()
        if self.proc is not None:
            try:
                self.proc.terminate()
                self.proc.wait(timeout=3)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait(timeout=3)
            self.proc = None
        for f in (self.stdout_file, self.stderr_file):
            if f is not None:
                f.close()
        self.stdout_file = None
        self.stderr_file = None


def checkpoint_key(row: dict[str, Any]) -> dict[str, Any]:
    keys = ("cp", "vis", "cycle", "cycle_count", "cpu_retired", "cpu_int", "cp0", "cp1", "rdram", "chain")
    return {k: row.get(k) for k in keys}


def collect_dump(inst: Instance, rows: int) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for name, payload in (
        ("chain", "cosim_chain"),
        ("sub", "cosim_sub"),
        ("quiescence", "cosim_quiescence"),
        ("threads", "cosim_threads"),
        ("window", {"cmd": "cosim_window", "n": rows}),
        ("regs", "cosim_regs"),
        ("status", "status"),
        ("libultra_recent", {"cmd": "libultra_recent", "n": 256}),
        ("mesg_dp_work", {"cmd": "mesg_queues", "mq": 0x8008468C, "tail": 64}),
        ("mesg_dp_done", {"cmd": "mesg_queues", "mq": 0x800846A4, "tail": 64}),
        ("mesg_gfx_task", {"cmd": "mesg_queues", "mq": 0x800846C4, "tail": 64}),
        ("mesg_scheduler", {"cmd": "mesg_queues", "mq": 0x800A6CD0, "tail": 64}),
    ):
        try:
            out[name] = inst.cmd(payload, timeout_s=10.0)
        except Exception as exc:  # diagnostic path, preserve partial evidence
            out[name] = {"ok": False, "error": str(exc)}
    return out


def paired_cmd(
    a: Instance,
    b: Instance,
    payload: str | dict[str, Any],
    timeout_s: float = 10.0,
) -> tuple[dict[str, Any], dict[str, Any]]:
    with ThreadPoolExecutor(max_workers=2) as pool:
        fa = pool.submit(a.cmd, payload, timeout_s)
        fb = pool.submit(b.cmd, payload, timeout_s)
        return fa.result(), fb.result()


def first_rdram_diff(
    a: Instance,
    b: Instance,
    *,
    normalized: bool = False,
    checkpoint: bool = False,
) -> dict[str, Any]:
    size = 8 * 1024 * 1024
    chunk = 0x40000
    if checkpoint:
        digest_cmd = "cosim_checkpoint_rdram_digest"
        peek_cmd = "cosim_checkpoint_rdram_peek"
    elif normalized:
        digest_cmd = "cosim_rdram_digest"
        peek_cmd = "cosim_rdram_peek"
    else:
        digest_cmd = "rdram_digest"
        peek_cmd = "rdram_peek"

    def digest(inst: Instance, start: int, count: int) -> dict[str, Any]:
        return inst.cmd(
            {"cmd": digest_cmd, "addr": start, "n": count},
            timeout_s=10.0,
        )

    def same_range(start: int, count: int) -> bool:
        da = digest(a, start, count)
        db = digest(b, start, count)
        if not da.get("ok") or not db.get("ok"):
            raise CosimError(f"{digest_cmd} failed at 0x{start:X}/0x{count:X}: {da} {db}")
        return da.get("fnv64") == db.get("fnv64")

    start = None
    for off in range(0, size, chunk):
        n = min(chunk, size - off)
        if not same_range(off, n):
            start = off
            count = n
            break
    if start is None:
        return {"ok": True, "different": False}

    while count > 1:
        half = count // 2
        if not same_range(start, half):
            count = half
        else:
            start += half
            count -= half

    peek_start = max(0, start - 16)
    peek_n = min(64, size - peek_start)
    pa = a.cmd({"cmd": peek_cmd, "addr": peek_start, "n": peek_n}, timeout_s=10.0)
    pb = b.cmd({"cmd": peek_cmd, "addr": peek_start, "n": peek_n}, timeout_s=10.0)
    return {
        "ok": True,
        "different": True,
        "paddr": start,
        "vaddr": 0x80000000 + start,
        "peek_start": peek_start,
        "peek_vaddr": 0x80000000 + peek_start,
        "peek_n": peek_n,
        "normalized": normalized,
        "checkpoint": checkpoint,
        "A": pa,
        "B": pb,
    }


def audit_checkpoint_rdram(a: Instance, b: Instance) -> dict[str, Any]:
    return first_rdram_diff(a, b, normalized=True, checkpoint=True)


def setup_pair(args: argparse.Namespace, gate_name: str) -> tuple[Instance, Instance, dict[str, Any], Path]:
    exe = host_path(args.exe)
    cwd = host_path(args.cwd)
    if not exe.exists():
        raise CosimError(f"missing exe: {exe}")

    log_dir = host_path(args.log_dir)
    a = Instance("A", exe, cwd, args.base_port, log_dir)
    b = Instance("B", exe, cwd, args.base_port + 1, log_dir)
    report_path = host_path(args.report)

    report: dict[str, Any] = {
        "ok": False,
        "gate": gate_name,
        "frames_requested": args.frames,
        "exe": str(exe),
        "cwd": str(cwd),
        "base_port": args.base_port,
        "rows": [],
    }
    return a, b, report, report_path


def start_and_reset_pair(a: Instance, b: Instance, args: argparse.Namespace) -> None:
    a.start()
    b.start()
    a.connect(args.startup_timeout)
    b.connect(args.startup_timeout)

    for inst in (a, b):
        pong = inst.cmd("ping")
        if not pong.get("ok"):
            raise CosimError(f"{inst.name} ping failed: {pong}")
        reset = inst.cmd("cosim_reset")
        if not reset.get("ok"):
            raise CosimError(f"{inst.name} reset failed: {reset}")

    start_a, start_b = paired_cmd(a, b, "cosim_start")
    if not start_a.get("ok"):
        raise CosimError(f"{a.name} start failed: {start_a}")
    if not start_b.get("ok"):
        raise CosimError(f"{b.name} start failed: {start_b}")


def write_report(path: Path, report: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, sort_keys=True), encoding="utf-8")


def run_gate(args: argparse.Namespace) -> int:
    a, b, report, report_path = setup_pair(args, "gate1_a_vs_a")
    report["audit_every"] = args.audit_every
    report["audits"] = []

    try:
        start_and_reset_pair(a, b, args)

        for frame in range(1, args.frames + 1):
            payload = {"cmd": "cosim_step", "frames": 1, "timeout_ms": args.step_timeout_ms}
            row_a, row_b = paired_cmd(
                a,
                b,
                payload,
                timeout_s=(args.step_timeout_ms / 1000.0) + 5.0,
            )
            rec = {
                "frame": frame,
                "A": checkpoint_key(row_a),
                "B": checkpoint_key(row_b),
            }
            report["rows"].append(rec)

            if not row_a.get("ok") or not row_b.get("ok"):
                report["error"] = "step failed"
                report["first_bad_frame"] = frame
                report["step_A"] = row_a
                report["step_B"] = row_b
                report["first_rdram_diff"] = first_rdram_diff(a, b)
                if checkpoint_key(row_a).get("rdram") != checkpoint_key(row_b).get("rdram"):
                    report["first_normalized_rdram_diff"] = first_rdram_diff(a, b, normalized=True, checkpoint=True)
                report["dump_A"] = collect_dump(a, args.window)
                report["dump_B"] = collect_dump(b, args.window)
                write_report(report_path, report)
                print(f"FAIL step frame={frame}; report={report_path}")
                return 1

            if checkpoint_key(row_a) != checkpoint_key(row_b):
                report["error"] = "checkpoint mismatch"
                report["first_bad_frame"] = frame
                report["step_A"] = row_a
                report["step_B"] = row_b
                report["first_rdram_diff"] = first_rdram_diff(a, b)
                if checkpoint_key(row_a).get("rdram") != checkpoint_key(row_b).get("rdram"):
                    report["first_normalized_rdram_diff"] = first_rdram_diff(a, b, normalized=True, checkpoint=True)
                report["dump_A"] = collect_dump(a, args.window)
                report["dump_B"] = collect_dump(b, args.window)
                write_report(report_path, report)
                print(f"FAIL mismatch frame={frame}; report={report_path}")
                return 1

            if args.audit_every > 0 and frame % args.audit_every == 0:
                audit = audit_checkpoint_rdram(a, b)
                audit["frame"] = frame
                report["audits"].append(audit)
                if audit.get("different"):
                    report["error"] = "hash-vs-byte audit mismatch"
                    report["first_bad_frame"] = frame
                    report["step_A"] = row_a
                    report["step_B"] = row_b
                    report["first_normalized_rdram_diff"] = audit
                    report["dump_A"] = collect_dump(a, args.window)
                    report["dump_B"] = collect_dump(b, args.window)
                    write_report(report_path, report)
                    print(f"FAIL audit frame={frame}; report={report_path}")
                    return 1

            if args.verbose:
                print(
                    f"frame={frame} cp={row_a.get('cp')} vis={row_a.get('vis')} "
                    f"cycle={row_a.get('cycle')} cpu_retired={row_a.get('cpu_retired')} "
                    f"chain={row_a.get('chain')}"
                )

        report["ok"] = True
        report["frames_completed"] = args.frames
        report["final"] = report["rows"][-1] if report["rows"] else None
        write_report(report_path, report)
        print(f"PASS gate1_a_vs_a frames={args.frames}; report={report_path}")
        return 0
    finally:
        a.stop()
        b.stop()


def run_gate3(args: argparse.Namespace) -> int:
    a, b, report, report_path = setup_pair(args, "gate3_injected_fault")
    expected_paddr = (args.inject_addr & 0x1FFFFFFF) ^ 3
    report["inject"] = {
        "side": "A",
        "field": "rdram_byte",
        "addr": args.inject_addr,
        "expected_paddr": expected_paddr,
        "value": args.inject_value & 0xFF,
        "after_frame": args.inject_frame,
        "expected_first_bad_frame": args.inject_frame + 1,
        "expected_subsystem": "rdram",
    }

    try:
        start_and_reset_pair(a, b, args)

        last_a: dict[str, Any] | None = None
        last_b: dict[str, Any] | None = None
        for frame in range(1, args.inject_frame + 1):
            row_a, row_b = paired_cmd(
                a,
                b,
                {"cmd": "cosim_step", "frames": 1, "timeout_ms": args.step_timeout_ms},
                timeout_s=(args.step_timeout_ms / 1000.0) + 5.0,
            )
            last_a, last_b = row_a, row_b
            report["rows"].append({"frame": frame, "A": checkpoint_key(row_a), "B": checkpoint_key(row_b)})
            if not row_a.get("ok") or not row_b.get("ok") or checkpoint_key(row_a) != checkpoint_key(row_b):
                report["error"] = "pre-injection determinism failed"
                report["first_bad_frame"] = frame
                report["step_A"] = row_a
                report["step_B"] = row_b
                report["first_rdram_diff"] = first_rdram_diff(a, b)
                report["dump_A"] = collect_dump(a, args.window)
                report["dump_B"] = collect_dump(b, args.window)
                write_report(report_path, report)
                print(f"FAIL gate3 pre-injection frame={frame}; report={report_path}")
                return 1

        inj = a.cmd(
            {
                "cmd": "cosim_inject",
                "field": "rdram_byte",
                "addr": args.inject_addr,
                "value": args.inject_value & 0xFF,
            },
            timeout_s=10.0,
        )
        report["inject_result"] = inj
        if not inj.get("ok"):
            report["error"] = "inject command failed"
            report["last_A"] = last_a
            report["last_B"] = last_b
            write_report(report_path, report)
            print(f"FAIL gate3 inject; report={report_path}")
            return 1

        for frame in range(args.inject_frame + 1, args.inject_frame + args.post_frames + 1):
            row_a, row_b = paired_cmd(
                a,
                b,
                {"cmd": "cosim_step", "frames": 1, "timeout_ms": args.step_timeout_ms},
                timeout_s=(args.step_timeout_ms / 1000.0) + 5.0,
            )
            rec = {"frame": frame, "A": checkpoint_key(row_a), "B": checkpoint_key(row_b)}
            report["rows"].append(rec)
            if not row_a.get("ok") or not row_b.get("ok"):
                report["error"] = "post-injection step failed"
                report["first_bad_frame"] = frame
                report["step_A"] = row_a
                report["step_B"] = row_b
                write_report(report_path, report)
                print(f"FAIL gate3 step frame={frame}; report={report_path}")
                return 1

            if checkpoint_key(row_a) != checkpoint_key(row_b):
                subdiff = [
                    key for key in ("cpu_int", "cp0", "cp1", "rdram")
                    if row_a.get(key) != row_b.get(key)
                ]
                diff = first_rdram_diff(a, b, normalized=True, checkpoint=True)
                report["ok"] = (
                    frame == args.inject_frame + 1 and
                    subdiff == ["rdram"] and
                    diff.get("different") is True and
                    diff.get("paddr") == expected_paddr
                )
                report["first_bad_frame"] = frame
                report["subdiff"] = subdiff
                report["step_A"] = row_a
                report["step_B"] = row_b
                report["first_normalized_rdram_diff"] = diff
                report["dump_A"] = collect_dump(a, args.window)
                report["dump_B"] = collect_dump(b, args.window)
                write_report(report_path, report)
                if report["ok"]:
                    print(f"PASS gate3_injected_fault frame={frame}; report={report_path}")
                    return 0
                print(f"FAIL gate3 wrong mismatch frame={frame}; report={report_path}")
                return 1

            if args.verbose:
                print(
                    f"frame={frame} cp={row_a.get('cp')} vis={row_a.get('vis')} "
                    f"chain={row_a.get('chain')}"
                )

        report["error"] = "injected fault was not detected"
        report["last_A"] = checkpoint_key(row_a)
        report["last_B"] = checkpoint_key(row_b)
        report["first_normalized_rdram_diff"] = first_rdram_diff(a, b, normalized=True, checkpoint=True)
        write_report(report_path, report)
        print(f"FAIL gate3 blind coordinator; report={report_path}")
        return 1
    finally:
        a.stop()
        b.stop()


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description="N64 differential co-sim coordinator")
    sub = ap.add_subparsers(dest="cmd", required=True)

    gate = sub.add_parser("gate1", help="run recomp-vs-recomp determinism gate")
    gate.add_argument("--frames", type=int, default=60)
    gate.add_argument("--step-timeout-ms", type=int, default=5000)
    gate.add_argument("--startup-timeout", type=float, default=30.0)
    gate.add_argument("--base-port", type=int, default=4610)
    gate.add_argument("--exe", default=str(DEFAULT_EXE))
    gate.add_argument("--cwd", default=str(BUILD))
    gate.add_argument("--log-dir", default=str(BUILD / "cosim_gate1_logs"))
    gate.add_argument("--report", default=str(BUILD / "cosim_gate1_report.json"))
    gate.add_argument("--window", type=int, default=16)
    gate.add_argument("--audit-every", type=int, default=0, help="full checkpoint RDRAM byte audit interval; 0 disables")
    gate.add_argument("--verbose", action="store_true")

    gate3 = sub.add_parser("gate3", help="run injected-fault coordinator sanity gate")
    gate3.add_argument("--frames", type=int, default=6)
    gate3.add_argument("--inject-frame", type=int, default=3)
    gate3.add_argument("--post-frames", type=int, default=3)
    gate3.add_argument("--inject-addr", type=lambda x: int(x, 0), default=0x807FF000)
    gate3.add_argument("--inject-value", type=lambda x: int(x, 0), default=0x5A)
    gate3.add_argument("--step-timeout-ms", type=int, default=5000)
    gate3.add_argument("--startup-timeout", type=float, default=30.0)
    gate3.add_argument("--base-port", type=int, default=4710)
    gate3.add_argument("--exe", default=str(DEFAULT_EXE))
    gate3.add_argument("--cwd", default=str(BUILD))
    gate3.add_argument("--log-dir", default=str(BUILD / "cosim_gate3_logs"))
    gate3.add_argument("--report", default=str(BUILD / "cosim_gate3_report.json"))
    gate3.add_argument("--window", type=int, default=16)
    gate3.add_argument("--verbose", action="store_true")

    args = ap.parse_args(argv)
    if args.cmd == "gate1":
        return run_gate(args)
    if args.cmd == "gate3":
        return run_gate3(args)
    raise AssertionError(args.cmd)


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except CosimError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)
