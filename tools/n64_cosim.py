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
DEFAULT_ROM = ROOT / "baserom.z64"
DEFAULT_ARES_EXE = (
    ROOT.parent
    / "N64Recomp-issues"
    / "build_ares_bridge"
    / "ares-bridge"
    / "Release"
    / "ares_oracle_server.exe"
)
RDRAM_SIZE = 8 * 1024 * 1024
RDRAM_CHUNK = 0x40000


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


class AresInstance:
    def __init__(self, name: str, exe: Path, rom: Path, cwd: Path, port: int, log_dir: Path) -> None:
        self.name = name
        self.exe = exe
        self.rom = rom
        self.cwd = cwd
        self.port = port
        self.log_dir = log_dir
        self.proc: subprocess.Popen[bytes] | None = None
        self.client = DebugClient("127.0.0.1", port)
        self.stdout_file = None
        self.stderr_file = None

    def start(self) -> None:
        self.log_dir.mkdir(parents=True, exist_ok=True)
        self.stdout_file = (self.log_dir / f"{self.name}.stdout.log").open("wb")
        self.stderr_file = (self.log_dir / f"{self.name}.stderr.log").open("wb")
        creationflags = getattr(subprocess, "CREATE_NO_WINDOW", 0)
        self.proc = subprocess.Popen(
            [str(self.exe), str(self.rom), str(self.port)],
            cwd=str(self.cwd),
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


def parse_int(value: Any) -> int:
    if isinstance(value, int):
        return value
    if isinstance(value, str):
        return int(value, 0)
    raise CosimError(f"cannot parse integer value {value!r}")


def compare_cpu_int(recomp_regs: dict[str, Any], ares_cpu: dict[str, Any]) -> dict[str, Any]:
    if not recomp_regs.get("ok"):
        return {"ok": False, "error": f"recomp regs failed: {recomp_regs}"}
    if not ares_cpu.get("ok"):
        return {"ok": False, "error": f"ares cpu failed: {ares_cpu}"}

    recomp_gpr = recomp_regs.get("gpr")
    ares_gpr = ares_cpu.get("gpr")
    if not isinstance(recomp_gpr, list) or len(recomp_gpr) != 32:
        return {"ok": False, "error": "recomp gpr payload missing/invalid"}
    if not isinstance(ares_gpr, list) or len(ares_gpr) != 32:
        return {"ok": False, "error": "ares gpr payload missing/invalid"}

    gpr_mismatches: list[dict[str, Any]] = []
    for i in range(32):
        rv = parse_int(recomp_gpr[i]) & 0xFFFFFFFFFFFFFFFF
        av = parse_int(ares_gpr[i]) & 0xFFFFFFFFFFFFFFFF
        if rv != av:
            gpr_mismatches.append(
                {
                    "reg": i,
                    "recomp": f"0x{rv:016x}",
                    "ares": f"0x{av:016x}",
                }
            )

    hi_recomp = parse_int(recomp_regs.get("hi", 0)) & 0xFFFFFFFFFFFFFFFF
    lo_recomp = parse_int(recomp_regs.get("lo", 0)) & 0xFFFFFFFFFFFFFFFF
    hi_ares = parse_int(ares_cpu.get("hi", 0)) & 0xFFFFFFFFFFFFFFFF
    lo_ares = parse_int(ares_cpu.get("lo", 0)) & 0xFFFFFFFFFFFFFFFF
    hi_lo_mismatches = []
    if hi_recomp != hi_ares:
        hi_lo_mismatches.append(
            {"reg": "hi", "recomp": f"0x{hi_recomp:016x}", "ares": f"0x{hi_ares:016x}"}
        )
    if lo_recomp != lo_ares:
        hi_lo_mismatches.append(
            {"reg": "lo", "recomp": f"0x{lo_recomp:016x}", "ares": f"0x{lo_ares:016x}"}
        )

    return {
        "ok": True,
        "different": bool(gpr_mismatches or hi_lo_mismatches),
        "gpr_mismatches": gpr_mismatches,
        "hi_lo_mismatches": hi_lo_mismatches,
        "ares_pc": ares_cpu.get("pc"),
    }


def first_recomp_ares_rdram_diff(recomp: Instance, ares: AresInstance) -> dict[str, Any]:
    def recomp_digest(start: int, count: int) -> dict[str, Any]:
        return recomp.cmd(
            {"cmd": "cosim_checkpoint_rdram_digest", "addr": start, "n": count},
            timeout_s=20.0,
        )

    def ares_digest(start: int, count: int) -> dict[str, Any]:
        return ares.cmd(
            {
                "cmd": "read_memory_digest",
                "addr": 0x80000000 + start,
                "len": count,
                "order": "recomp",
            },
            timeout_s=60.0,
        )

    def same_range(start: int, count: int) -> bool:
        dr = recomp_digest(start, count)
        da = ares_digest(start, count)
        if not dr.get("ok") or not da.get("ok"):
            raise CosimError(f"RDRAM digest failed at 0x{start:X}/0x{count:X}: {dr} {da}")
        return dr.get("fnv64") == da.get("fnv64")

    start = None
    for off in range(0, RDRAM_SIZE, RDRAM_CHUNK):
        n = min(RDRAM_CHUNK, RDRAM_SIZE - off)
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
    peek_n = min(64, RDRAM_SIZE - peek_start)
    pr = recomp.cmd(
        {"cmd": "cosim_checkpoint_rdram_peek", "addr": peek_start, "n": peek_n},
        timeout_s=10.0,
    )
    pa = ares.cmd(
        {
            "cmd": "read_memory",
            "addr": 0x80000000 + peek_start,
            "len": peek_n,
            "order": "recomp",
        },
        timeout_s=20.0,
    )
    return {
        "ok": True,
        "different": True,
        "paddr": start,
        "vaddr": 0x80000000 + start,
        "peek_start": peek_start,
        "peek_vaddr": 0x80000000 + peek_start,
        "peek_n": peek_n,
        "order": "recomp",
        "recomp": pr,
        "ares": pa,
    }


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


def collect_ares_dump(ares: AresInstance) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for name, payload in (
        ("status", "status"),
        ("cpu", {"cmd": "read_cpu_state"}),
        ("rsp_trace_recent", {"cmd": "rsp_trace_recent", "n": 256}),
    ):
        try:
            out[name] = ares.cmd(payload, timeout_s=20.0)
        except Exception as exc:
            out[name] = {"ok": False, "error": str(exc)}
    return out


def run_ares_smoke(args: argparse.Namespace) -> int:
    ares_exe = host_path(args.ares_exe)
    rom = host_path(args.rom)
    ares_cwd = host_path(args.ares_cwd)
    log_dir = host_path(args.log_dir)
    report_path = host_path(args.report)
    if not ares_exe.exists():
        raise CosimError(f"missing Ares oracle server: {ares_exe}")
    if not rom.exists():
        raise CosimError(f"missing ROM: {rom}")

    ares = AresInstance("ares", ares_exe, rom, ares_cwd, args.port, log_dir)
    report: dict[str, Any] = {
        "ok": False,
        "gate": "ares_smoke",
        "ares_exe": str(ares_exe),
        "rom": str(rom),
        "port": args.port,
        "frames_requested": args.frames,
        "rows": [],
    }

    try:
        ares.start()
        ares.connect(args.startup_timeout)
        pong = ares.cmd("ping")
        status = ares.cmd("status")
        report["ping"] = pong
        report["status"] = status
        if not pong.get("ok") or not status.get("ok") or not status.get("is_real"):
            report["error"] = "Ares server did not report a real oracle"
            write_report(report_path, report)
            print(f"FAIL ares_smoke init; report={report_path}")
            return 1

        reset = ares.cmd("reset", timeout_s=30.0)
        report["reset"] = reset
        if not reset.get("ok"):
            report["error"] = "Ares reset failed"
            write_report(report_path, report)
            print(f"FAIL ares_smoke reset; report={report_path}")
            return 1

        for frame in range(1, args.frames + 1):
            step = ares.cmd({"cmd": "step_frame", "n": 1}, timeout_s=args.step_timeout)
            cpu = ares.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
            digest = ares.cmd(
                {
                    "cmd": "read_memory_digest",
                    "addr": 0x80000000,
                    "len": min(args.digest_len, RDRAM_SIZE),
                    "order": "recomp",
                },
                timeout_s=60.0,
            )
            row = {"frame": frame, "step": step, "cpu": cpu, "rdram_digest": digest}
            report["rows"].append(row)
            if not step.get("ok") or not cpu.get("ok") or not digest.get("ok"):
                report["error"] = "Ares smoke query failed"
                report["first_bad_frame"] = frame
                write_report(report_path, report)
                print(f"FAIL ares_smoke frame={frame}; report={report_path}")
                return 1
            if args.verbose:
                print(
                    f"frame={frame} pc={cpu.get('pc')} "
                    f"rdram_fnv={digest.get('fnv64')} trace={step.get('trace_count')}"
                )

        report["ok"] = True
        report["frames_completed"] = args.frames
        write_report(report_path, report)
        print(f"PASS ares_smoke frames={args.frames}; report={report_path}")
        return 0
    finally:
        ares.stop()


def run_oracle(args: argparse.Namespace) -> int:
    exe = host_path(args.exe)
    cwd = host_path(args.cwd)
    ares_exe = host_path(args.ares_exe)
    rom = host_path(args.rom)
    ares_cwd = host_path(args.ares_cwd)
    log_dir = host_path(args.log_dir)
    report_path = host_path(args.report)
    if not exe.exists():
        raise CosimError(f"missing recomp exe: {exe}")
    if not ares_exe.exists():
        raise CosimError(f"missing Ares oracle server: {ares_exe}")
    if not rom.exists():
        raise CosimError(f"missing ROM: {rom}")

    recomp = Instance("recomp", exe, cwd, args.base_port, log_dir)
    ares = AresInstance("ares", ares_exe, rom, ares_cwd, args.base_port + 1, log_dir)
    report: dict[str, Any] = {
        "ok": False,
        "gate": "oracle_recomp_vs_ares",
        "frames_requested": args.frames,
        "exe": str(exe),
        "cwd": str(cwd),
        "ares_exe": str(ares_exe),
        "rom": str(rom),
        "base_port": args.base_port,
        "coverage": {
            "axis": "per-VI-frame modeled cycle checkpoints",
            "compared": ["cpu_gpr_hi_lo", "rdram_8mb_recomp_byte_order"],
            "pending": ["cp0", "cp1/fpr", "pc/recomp_context_surface"],
        },
        "rows": [],
    }

    try:
        recomp.start()
        ares.start()
        recomp.connect(args.startup_timeout)
        ares.connect(args.startup_timeout)

        recomp_ping = recomp.cmd("ping")
        ares_ping = ares.cmd("ping")
        ares_status = ares.cmd("status")
        report["recomp_ping"] = recomp_ping
        report["ares_ping"] = ares_ping
        report["ares_status"] = ares_status
        if not recomp_ping.get("ok") or not ares_ping.get("ok") or not ares_status.get("is_real"):
            report["error"] = "init ping/status failed"
            write_report(report_path, report)
            print(f"FAIL oracle init; report={report_path}")
            return 1

        recomp_reset = recomp.cmd("cosim_reset")
        ares_reset = ares.cmd("reset", timeout_s=30.0)
        report["recomp_reset"] = recomp_reset
        report["ares_reset"] = ares_reset
        if not recomp_reset.get("ok") or not ares_reset.get("ok"):
            report["error"] = "reset failed"
            write_report(report_path, report)
            print(f"FAIL oracle reset; report={report_path}")
            return 1

        report["ares_warmup_frames"] = args.ares_warmup_frames
        report["ares_warmup"] = []
        for warmup in range(1, args.ares_warmup_frames + 1):
            step = ares.cmd({"cmd": "step_frame", "n": 1}, timeout_s=args.ares_step_timeout)
            cpu = ares.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
            rec = {"frame": warmup, "step": step, "pc": cpu.get("pc"), "cpu_ok": cpu.get("ok")}
            report["ares_warmup"].append(rec)
            if not step.get("ok") or not cpu.get("ok"):
                report["error"] = "Ares warmup failed"
                report["first_bad_frame"] = warmup
                write_report(report_path, report)
                print(f"FAIL oracle Ares warmup frame={warmup}; report={report_path}")
                return 1

        start = recomp.cmd("cosim_start")
        report["recomp_start"] = start
        if not start.get("ok"):
            report["error"] = "recomp cosim_start failed"
            write_report(report_path, report)
            print(f"FAIL oracle start; report={report_path}")
            return 1

        for frame in range(1, args.frames + 1):
            with ThreadPoolExecutor(max_workers=2) as pool:
                fr = pool.submit(
                    recomp.cmd,
                    {"cmd": "cosim_step", "frames": 1, "timeout_ms": args.step_timeout_ms},
                    (args.step_timeout_ms / 1000.0) + 5.0,
                )
                fa = pool.submit(ares.cmd, {"cmd": "step_frame", "n": 1}, args.ares_step_timeout)
                recomp_step = fr.result()
                ares_step = fa.result()

            rec: dict[str, Any] = {
                "frame": frame,
                "recomp": checkpoint_key(recomp_step),
                "ares_step": ares_step,
            }
            report["rows"].append(rec)
            if not recomp_step.get("ok") or not ares_step.get("ok"):
                report["error"] = "step failed"
                report["first_bad_frame"] = frame
                report["step_recomp"] = recomp_step
                report["step_ares"] = ares_step
                report["dump_recomp"] = collect_dump(recomp, args.window)
                report["dump_ares"] = collect_ares_dump(ares)
                write_report(report_path, report)
                print(f"FAIL oracle step frame={frame}; report={report_path}")
                return 1

            recomp_regs = recomp.cmd("cosim_regs", timeout_s=10.0)
            ares_cpu = ares.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
            cpu_diff = compare_cpu_int(recomp_regs, ares_cpu)
            rec["cpu_diff"] = cpu_diff

            rdram_diff = {"ok": True, "different": False, "skipped": True}
            if args.rdram_every > 0 and frame % args.rdram_every == 0:
                rdram_diff = first_recomp_ares_rdram_diff(recomp, ares)
            rec["rdram_diff"] = {
                k: v for k, v in rdram_diff.items()
                if k not in ("recomp", "ares")
            }

            subdiff = []
            if not cpu_diff.get("ok") or cpu_diff.get("different"):
                subdiff.append("cpu_int")
            if rdram_diff.get("different"):
                subdiff.append("rdram")

            if subdiff:
                report["error"] = "oracle mismatch"
                report["first_bad_frame"] = frame
                report["subdiff"] = subdiff
                report["step_recomp"] = recomp_step
                report["step_ares"] = ares_step
                report["cpu_diff"] = cpu_diff
                report["rdram_diff"] = rdram_diff
                report["dump_recomp"] = collect_dump(recomp, args.window)
                report["dump_ares"] = collect_ares_dump(ares)
                write_report(report_path, report)
                print(f"FAIL oracle mismatch frame={frame} subdiff={','.join(subdiff)}; report={report_path}")
                return 1

            if args.verbose:
                print(
                    f"frame={frame} cp={recomp_step.get('cp')} vis={recomp_step.get('vis')} "
                    f"cycle={recomp_step.get('cycle')} ares_trace={ares_step.get('trace_count')}"
                )

        report["ok"] = True
        report["frames_completed"] = args.frames
        report["final"] = report["rows"][-1] if report["rows"] else None
        write_report(report_path, report)
        print(f"PASS oracle_recomp_vs_ares frames={args.frames}; report={report_path}")
        return 0
    finally:
        recomp.stop()
        ares.stop()


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

    ares_smoke = sub.add_parser("ares-smoke", help="launch and query the standalone Ares oracle")
    ares_smoke.add_argument("--frames", type=int, default=2)
    ares_smoke.add_argument("--port", type=int, default=4872)
    ares_smoke.add_argument("--startup-timeout", type=float, default=30.0)
    ares_smoke.add_argument("--step-timeout", type=float, default=90.0)
    ares_smoke.add_argument("--digest-len", type=int, default=0x10000)
    ares_smoke.add_argument("--ares-exe", default=str(DEFAULT_ARES_EXE))
    ares_smoke.add_argument("--ares-cwd", default=str(DEFAULT_ARES_EXE.parent))
    ares_smoke.add_argument("--rom", default=str(DEFAULT_ROM))
    ares_smoke.add_argument("--log-dir", default=str(BUILD / "cosim_ares_smoke_logs"))
    ares_smoke.add_argument("--report", default=str(BUILD / "cosim_ares_smoke_report.json"))
    ares_smoke.add_argument("--verbose", action="store_true")

    oracle = sub.add_parser("oracle", help="run recomp-vs-Ares per-VI checkpoint diff")
    oracle.add_argument("--frames", type=int, default=60)
    oracle.add_argument("--step-timeout-ms", type=int, default=15000)
    oracle.add_argument("--ares-step-timeout", type=float, default=90.0)
    oracle.add_argument("--ares-warmup-frames", type=int, default=13, help="step Ares this many VI frames before starting recomp")
    oracle.add_argument("--startup-timeout", type=float, default=30.0)
    oracle.add_argument("--base-port", type=int, default=4890)
    oracle.add_argument("--exe", default=str(DEFAULT_EXE))
    oracle.add_argument("--cwd", default=str(BUILD))
    oracle.add_argument("--ares-exe", default=str(DEFAULT_ARES_EXE))
    oracle.add_argument("--ares-cwd", default=str(DEFAULT_ARES_EXE.parent))
    oracle.add_argument("--rom", default=str(DEFAULT_ROM))
    oracle.add_argument("--log-dir", default=str(BUILD / "cosim_oracle_logs"))
    oracle.add_argument("--report", default=str(BUILD / "cosim_oracle_report.json"))
    oracle.add_argument("--window", type=int, default=16)
    oracle.add_argument("--rdram-every", type=int, default=1, help="compare full RDRAM every N frames; 0 disables")
    oracle.add_argument("--verbose", action="store_true")

    args = ap.parse_args(argv)
    if args.cmd == "gate1":
        return run_gate(args)
    if args.cmd == "gate3":
        return run_gate3(args)
    if args.cmd == "ares-smoke":
        return run_ares_smoke(args)
    if args.cmd == "oracle":
        return run_oracle(args)
    raise AssertionError(args.cmd)


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except CosimError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)
