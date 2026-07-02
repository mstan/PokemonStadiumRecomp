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


def stop_process(proc: subprocess.Popen[bytes], timeout_s: float = 3.0) -> None:
    try:
        proc.terminate()
    except (OSError, ProcessLookupError, PermissionError):
        pass
    try:
        proc.wait(timeout=timeout_s)
        return
    except subprocess.TimeoutExpired:
        pass

    try:
        proc.kill()
    except (OSError, ProcessLookupError, PermissionError):
        try:
            subprocess.run(
                ["cmd.exe", "/c", "taskkill", "/PID", str(proc.pid), "/T", "/F"],
                stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=5,
                check=False,
            )
        except (OSError, subprocess.TimeoutExpired):
            pass
    try:
        proc.wait(timeout=timeout_s)
    except (OSError, subprocess.TimeoutExpired, PermissionError):
        pass


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
            stop_process(self.proc)
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
            stop_process(self.proc)
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
        ("rcp", "cosim_rcp"),
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


MAX_CPU_MISMATCHES = 16
CP0_COARSE_IGNORE_REGS = {1}  # Random is not stable at coarse VI readback.


def hex_value(value: int, bits: int) -> str:
    width = max(1, bits // 4)
    mask = (1 << bits) - 1
    return f"0x{value & mask:0{width}x}"


def compare_scalar(
    name: str,
    left: Any,
    right: Any,
    *,
    left_name: str,
    right_name: str,
    bits: int,
) -> dict[str, Any] | None:
    lv = parse_int(left)
    rv = parse_int(right)
    if (lv & ((1 << bits) - 1)) == (rv & ((1 << bits) - 1)):
        return None
    return {
        "reg": name,
        left_name: hex_value(lv, bits),
        right_name: hex_value(rv, bits),
    }


def compare_register_list(
    left_regs: Any,
    right_regs: Any,
    *,
    left_name: str,
    right_name: str,
    bits: int,
    value_names: tuple[str, str],
    ignore_regs: set[int] | None = None,
    limit: int = MAX_CPU_MISMATCHES,
) -> tuple[dict[str, Any] | None, list[dict[str, Any]], int]:
    if not isinstance(left_regs, list) or len(left_regs) != 32:
        return {"ok": False, "error": f"{left_name} register payload missing/invalid"}, [], 0
    if not isinstance(right_regs, list) or len(right_regs) != 32:
        return {"ok": False, "error": f"{right_name} register payload missing/invalid"}, [], 0

    mismatches: list[dict[str, Any]] = []
    total = 0
    mask = (1 << bits) - 1
    ignored = ignore_regs or set()
    for i in range(32):
        if i in ignored:
            continue
        lv = parse_int(left_regs[i]) & mask
        rv = parse_int(right_regs[i]) & mask
        if lv == rv:
            continue
        total += 1
        if len(mismatches) < limit:
            mismatches.append(
                {
                    "reg": i,
                    value_names[0]: hex_value(lv, bits),
                    value_names[1]: hex_value(rv, bits),
                }
            )
    return None, mismatches, total


def synth_recomp_cp0(recomp_regs: dict[str, Any]) -> list[int] | None:
    cp0 = recomp_regs.get("cp0")
    if not isinstance(cp0, list) or len(cp0) != 32:
        return None
    out = [parse_int(v) & 0xFFFFFFFF for v in cp0]
    if "status_reg" in recomp_regs:
        out[12] = parse_int(recomp_regs["status_reg"]) & 0xFFFFFFFF
    return out


def compare_cpu_int(recomp_regs: dict[str, Any], ares_cpu: dict[str, Any]) -> dict[str, Any]:
    if not recomp_regs.get("ok"):
        return {"ok": False, "error": f"recomp regs failed: {recomp_regs}"}
    if not ares_cpu.get("ok"):
        return {"ok": False, "error": f"ares cpu failed: {ares_cpu}"}

    err, gpr_mismatches, gpr_total = compare_register_list(
        recomp_regs.get("gpr"),
        ares_cpu.get("gpr"),
        left_name="recomp gpr",
        right_name="ares gpr",
        bits=64,
        value_names=("recomp", "ares"),
    )
    if err:
        return err

    hi_lo_mismatches = []
    for reg in ("hi", "lo"):
        mismatch = compare_scalar(
            reg,
            recomp_regs.get(reg, 0),
            ares_cpu.get(reg, 0),
            left_name="recomp",
            right_name="ares",
            bits=64,
        )
        if mismatch:
            hi_lo_mismatches.append(mismatch)

    recomp_cp0 = synth_recomp_cp0(recomp_regs)
    err, cp0_mismatches, cp0_total = compare_register_list(
        recomp_cp0,
        ares_cpu.get("cp0"),
        left_name="recomp cp0",
        right_name="ares cp0",
        bits=32,
        value_names=("recomp", "ares"),
        ignore_regs=CP0_COARSE_IGNORE_REGS,
    )
    if err:
        return err

    err, fpr_mismatches, fpr_total = compare_register_list(
        recomp_regs.get("fpr"),
        ares_cpu.get("fpr"),
        left_name="recomp fpr",
        right_name="ares fpr",
        bits=64,
        value_names=("recomp", "ares"),
    )
    if err:
        return err

    fpu_control_mismatches: list[dict[str, Any]] = []
    if "mips3_float_mode" in recomp_regs and isinstance(ares_cpu.get("cp0"), list):
        status = parse_int(ares_cpu["cp0"][12]) & 0xFFFFFFFF
        recomp_fr = parse_int(recomp_regs.get("mips3_float_mode", 0)) & 1
        ares_fr = (status >> 26) & 1
        if recomp_fr != ares_fr:
            fpu_control_mismatches.append(
                {"reg": "status.fr", "recomp": str(recomp_fr), "ares": str(ares_fr)}
            )
    if "cop1_cs" in recomp_regs and "fcsr" in ares_cpu:
        recomp_round = parse_int(recomp_regs["cop1_cs"]) & 0x3
        ares_round = parse_int(ares_cpu["fcsr"]) & 0x3
        if recomp_round != ares_round:
            fpu_control_mismatches.append(
                {
                    "reg": "fcsr.round",
                    "recomp": hex_value(recomp_round, 2),
                    "ares": hex_value(ares_round, 2),
                    "ares_fcsr": hex_value(parse_int(ares_cpu["fcsr"]), 32),
                }
            )

    subdiff = []
    if gpr_mismatches or hi_lo_mismatches:
        subdiff.append("cpu_int")
    if cp0_mismatches:
        subdiff.append("cp0")
    if fpr_mismatches or fpu_control_mismatches:
        subdiff.append("cp1")

    return {
        "ok": True,
        "different": bool(subdiff),
        "subdiff": subdiff,
        "gpr_mismatches": gpr_mismatches,
        "gpr_mismatch_count": gpr_total,
        "hi_lo_mismatches": hi_lo_mismatches,
        "cp0_mismatches": cp0_mismatches,
        "cp0_mismatch_count": cp0_total,
        "cp0_ignored_regs": sorted(CP0_COARSE_IGNORE_REGS),
        "fpr_mismatches": fpr_mismatches,
        "fpr_mismatch_count": fpr_total,
        "fpu_control_mismatches": fpu_control_mismatches,
        "ares_fcr0": ares_cpu.get("fcr0"),
        "ares_fcsr": ares_cpu.get("fcsr"),
        "recomp_cop1_cs": recomp_regs.get("cop1_cs"),
        "recomp_status_reg": recomp_regs.get("status_reg"),
        "recomp_mips3_float_mode": recomp_regs.get("mips3_float_mode"),
        "ares_pc": ares_cpu.get("pc"),
    }


def first_recomp_ares_rdram_diff(
    recomp: Instance,
    ares: AresInstance,
    start_offset: int = 0,
    end_offset: int = RDRAM_SIZE,
) -> dict[str, Any]:
    if start_offset < 0 or end_offset > RDRAM_SIZE or start_offset >= end_offset:
        raise CosimError(f"invalid RDRAM search range 0x{start_offset:X}..0x{end_offset:X}")

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
    for off in range(start_offset, end_offset, RDRAM_CHUNK):
        n = min(RDRAM_CHUNK, end_offset - off)
        if not same_range(off, n):
            start = off
            count = n
            break
    if start is None:
        return {
            "ok": True,
            "different": False,
            "search_start": start_offset,
            "search_end": end_offset,
        }

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
        "search_start": start_offset,
        "search_end": end_offset,
        "paddr": start,
        "vaddr": 0x80000000 + start,
        "peek_start": peek_start,
        "peek_vaddr": 0x80000000 + peek_start,
        "peek_n": peek_n,
        "order": "recomp",
        "recomp": pr,
        "ares": pa,
    }


def rdram_offset(value: Any) -> int:
    raw = parse_int(value)
    if 0 <= raw < RDRAM_SIZE:
        return raw
    if 0x80000000 <= raw < 0x80000000 + RDRAM_SIZE:
        return raw - 0x80000000
    if 0xA0000000 <= raw < 0xA0000000 + RDRAM_SIZE:
        return raw - 0xA0000000
    raise CosimError(f"address is outside RDRAM: 0x{raw:X}")


def parse_rdram_range(value: str) -> tuple[str, int, int]:
    parts = value.split(":")
    if len(parts) != 3 or not parts[0]:
        raise CosimError(f"range must be name:start:end, got {value!r}")
    name = parts[0]
    start = rdram_offset(parts[1])
    end = rdram_offset(parts[2])
    if end <= start:
        raise CosimError(f"range {name!r} end must be after start")
    return name, start, end


def summarize_recomp_ares_rdram_diff(diff: dict[str, Any], *, include_bytes: bool = False) -> dict[str, Any]:
    keys = (
        "ok",
        "different",
        "search_start",
        "search_end",
        "paddr",
        "vaddr",
        "peek_start",
        "peek_vaddr",
        "peek_n",
        "n",
        "order",
    )
    out = {k: diff[k] for k in keys if k in diff}
    if include_bytes:
        recomp_peek = diff.get("recomp")
        ares_peek = diff.get("ares")
        if isinstance(recomp_peek, dict) and "hex" in recomp_peek:
            out["recomp_hex"] = recomp_peek.get("hex")
        if isinstance(ares_peek, dict) and "bytes" in ares_peek:
            out["ares_hex"] = ares_peek.get("bytes")
    return out


def peek_recomp_ares_rdram(
    recomp: Instance,
    ares: AresInstance,
    paddr: int,
    count: int,
) -> dict[str, Any]:
    if paddr < 0 or count < 1 or paddr + count > RDRAM_SIZE:
        raise CosimError(f"invalid RDRAM peek 0x{paddr:X}/0x{count:X}")
    pr = recomp.cmd(
        {"cmd": "cosim_checkpoint_rdram_peek", "addr": paddr, "n": count},
        timeout_s=10.0,
    )
    pa = ares.cmd(
        {
            "cmd": "read_memory",
            "addr": 0x80000000 + paddr,
            "len": count,
            "order": "recomp",
        },
        timeout_s=20.0,
    )
    recomp_hex = pr.get("hex") if isinstance(pr, dict) else None
    ares_hex = pa.get("bytes") if isinstance(pa, dict) else None
    return {
        "ok": bool(pr.get("ok") and pa.get("ok")),
        "different": recomp_hex != ares_hex,
        "paddr": paddr,
        "vaddr": 0x80000000 + paddr,
        "n": count,
        "recomp": pr,
        "ares": pa,
    }


def compare_ares_cpu(a_cpu: dict[str, Any], b_cpu: dict[str, Any]) -> dict[str, Any]:
    if not a_cpu.get("ok"):
        return {"ok": False, "error": f"Ares A cpu failed: {a_cpu}"}
    if not b_cpu.get("ok"):
        return {"ok": False, "error": f"Ares B cpu failed: {b_cpu}"}

    scalar_mismatches: list[dict[str, Any]] = []
    for reg, bits in (("pc", 32), ("hi", 64), ("lo", 64)):
        mismatch = compare_scalar(
            reg,
            a_cpu.get(reg, 0),
            b_cpu.get(reg, 0),
            left_name="A",
            right_name="B",
            bits=bits,
        )
        if mismatch:
            scalar_mismatches.append(mismatch)

    fpu_control_mismatches: list[dict[str, Any]] = []
    for reg in ("fcr0", "fcsr"):
        mismatch = compare_scalar(
            reg,
            a_cpu.get(reg, 0),
            b_cpu.get(reg, 0),
            left_name="A",
            right_name="B",
            bits=32,
        )
        if mismatch:
            fpu_control_mismatches.append(mismatch)

    err, gpr_mismatches, gpr_total = compare_register_list(
        a_cpu.get("gpr"),
        b_cpu.get("gpr"),
        left_name="Ares A gpr",
        right_name="Ares B gpr",
        bits=64,
        value_names=("A", "B"),
    )
    if err:
        return err
    err, cp0_mismatches, cp0_total = compare_register_list(
        a_cpu.get("cp0"),
        b_cpu.get("cp0"),
        left_name="Ares A cp0",
        right_name="Ares B cp0",
        bits=64,
        value_names=("A", "B"),
        ignore_regs=CP0_COARSE_IGNORE_REGS,
    )
    if err:
        return err
    err, fpr_mismatches, fpr_total = compare_register_list(
        a_cpu.get("fpr"),
        b_cpu.get("fpr"),
        left_name="Ares A fpr",
        right_name="Ares B fpr",
        bits=64,
        value_names=("A", "B"),
    )
    if err:
        return err

    subdiff = []
    if scalar_mismatches or gpr_mismatches:
        subdiff.append("cpu_int")
    if cp0_mismatches:
        subdiff.append("cp0")
    if fpr_mismatches or fpu_control_mismatches:
        subdiff.append("cp1")

    return {
        "ok": True,
        "different": bool(subdiff),
        "subdiff": subdiff,
        "scalar_mismatches": scalar_mismatches,
        "gpr_mismatches": gpr_mismatches,
        "gpr_mismatch_count": gpr_total,
        "cp0_mismatches": cp0_mismatches,
        "cp0_mismatch_count": cp0_total,
        "cp0_ignored_regs": sorted(CP0_COARSE_IGNORE_REGS),
        "fpr_mismatches": fpr_mismatches,
        "fpr_mismatch_count": fpr_total,
        "fpu_control_mismatches": fpu_control_mismatches,
        "pc": a_cpu.get("pc"),
    }


def first_ares_ares_rdram_diff(a: AresInstance, b: AresInstance) -> dict[str, Any]:
    def digest(inst: AresInstance, start: int, count: int) -> dict[str, Any]:
        return inst.cmd(
            {
                "cmd": "read_memory_digest",
                "addr": 0x80000000 + start,
                "len": count,
                "order": "recomp",
            },
            timeout_s=60.0,
        )

    def same_range(start: int, count: int) -> bool:
        da = digest(a, start, count)
        db = digest(b, start, count)
        if not da.get("ok") or not db.get("ok"):
            raise CosimError(f"Ares RDRAM digest failed at 0x{start:X}/0x{count:X}: {da} {db}")
        return da.get("fnv64") == db.get("fnv64")

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
    pa = a.cmd(
        {"cmd": "read_memory", "addr": 0x80000000 + peek_start, "len": peek_n, "order": "recomp"},
        timeout_s=20.0,
    )
    pb = b.cmd(
        {"cmd": "read_memory", "addr": 0x80000000 + peek_start, "len": peek_n, "order": "recomp"},
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
        "A": pa,
        "B": pb,
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


def run_ares_gate(args: argparse.Namespace) -> int:
    ares_exe = host_path(args.ares_exe)
    rom = host_path(args.rom)
    ares_cwd = host_path(args.ares_cwd)
    log_dir = host_path(args.log_dir)
    report_path = host_path(args.report)
    if not ares_exe.exists():
        raise CosimError(f"missing Ares oracle server: {ares_exe}")
    if not rom.exists():
        raise CosimError(f"missing ROM: {rom}")

    a = AresInstance("aresA", ares_exe, rom, ares_cwd, args.base_port, log_dir)
    b = AresInstance("aresB", ares_exe, rom, ares_cwd, args.base_port + 1, log_dir)
    report: dict[str, Any] = {
        "ok": False,
        "gate": "ares_vs_ares",
        "ares_exe": str(ares_exe),
        "rom": str(rom),
        "base_port": args.base_port,
        "frames_requested": args.frames,
        "rdram_every": args.rdram_every,
        "coverage": {
            "axis": "Ares step_frame VI frames",
            "compared": ["pc_hi_lo_gpr_cp0_stable_fpr_fcr", "rdram_8mb_recomp_byte_order"],
            "normalized_out": ["cp0_random"],
        },
        "rows": [],
    }

    try:
        a.start()
        b.start()
        a.connect(args.startup_timeout)
        b.connect(args.startup_timeout)

        init: dict[str, Any] = {}
        for inst in (a, b):
            ping = inst.cmd("ping")
            status = inst.cmd("status")
            reset = inst.cmd("reset", timeout_s=30.0)
            init[inst.name] = {"ping": ping, "status": status, "reset": reset}
            if not ping.get("ok") or not status.get("ok") or not status.get("is_real"):
                report["error"] = f"{inst.name} did not report a real oracle"
                report["init"] = init
                write_report(report_path, report)
                print(f"FAIL ares_vs_ares init; report={report_path}")
                return 1
            if not reset.get("ok"):
                report["error"] = f"{inst.name} reset failed"
                report["init"] = init
                write_report(report_path, report)
                print(f"FAIL ares_vs_ares reset; report={report_path}")
                return 1
        report["init"] = init

        for frame in range(1, args.frames + 1):
            with ThreadPoolExecutor(max_workers=2) as pool:
                fa = pool.submit(a.cmd, {"cmd": "step_frame", "n": 1}, args.step_timeout)
                fb = pool.submit(b.cmd, {"cmd": "step_frame", "n": 1}, args.step_timeout)
                step_a = fa.result()
                step_b = fb.result()

            cpu_a = a.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
            cpu_b = b.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
            cpu_diff = compare_ares_cpu(cpu_a, cpu_b)
            rdram_diff = {"ok": True, "different": False, "skipped": True}
            if args.rdram_every > 0 and frame % args.rdram_every == 0:
                rdram_diff = first_ares_ares_rdram_diff(a, b)

            rec: dict[str, Any] = {
                "frame": frame,
                "A_step": step_a,
                "B_step": step_b,
                "cpu_diff": cpu_diff,
                "rdram_diff": {k: v for k, v in rdram_diff.items() if k not in ("A", "B")},
            }
            report["rows"].append(rec)

            subdiff = []
            if not step_a.get("ok") or not step_b.get("ok"):
                subdiff.append("step")
            if not cpu_diff.get("ok") or cpu_diff.get("different"):
                cpu_subdiff = cpu_diff.get("subdiff")
                if isinstance(cpu_subdiff, list) and cpu_subdiff:
                    subdiff.extend(cpu_subdiff)
                else:
                    subdiff.append("cpu")
            if rdram_diff.get("different"):
                subdiff.append("rdram")
            if subdiff:
                report["error"] = "Ares determinism mismatch"
                report["first_bad_frame"] = frame
                report["subdiff"] = subdiff
                report["cpu_A"] = cpu_a
                report["cpu_B"] = cpu_b
                report["rdram_diff"] = rdram_diff
                write_report(report_path, report)
                print(f"FAIL ares_vs_ares frame={frame} subdiff={','.join(subdiff)}; report={report_path}")
                return 1

            if args.verbose:
                print(
                    f"frame={frame} pc={cpu_diff.get('pc')} "
                    f"traceA={step_a.get('trace_count')} traceB={step_b.get('trace_count')}"
                )

        report["ok"] = True
        report["frames_completed"] = args.frames
        report["final"] = report["rows"][-1] if report["rows"] else None
        write_report(report_path, report)
        print(f"PASS ares_vs_ares frames={args.frames}; report={report_path}")
        return 0
    finally:
        a.stop()
        b.stop()


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
            "compared": ["rdram_8mb_recomp_byte_order"],
            "cpu_compare": args.cpu_compare,
            "rdram_search_start": args.rdram_search_start,
            "rdram_search_end": args.rdram_search_end,
            "pending": ["pc/recomp_context_surface", "cp0_random", "full_fcsr_recomp_surface"],
        },
        "rows": [],
    }
    if args.cpu_compare == "fail":
        report["coverage"]["compared"].append("cpu_gpr_hi_lo_cp0_stable_fpr_status_fr_cop1_rounding")
    elif args.cpu_compare == "report":
        report["coverage"]["reported_not_gating"] = ["cpu_gpr_hi_lo_cp0_stable_fpr_status_fr_cop1_rounding"]
    if args.cpu_compare != "off":
        report["coverage"]["normalized_out"] = ["cp0_random"]

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

        report["recomp_warmup_frames"] = args.recomp_warmup_frames
        report["recomp_warmup"] = []
        for warmup in range(1, args.recomp_warmup_frames + 1):
            step = recomp.cmd(
                {"cmd": "cosim_step", "frames": 1, "timeout_ms": args.step_timeout_ms},
                timeout_s=(args.step_timeout_ms / 1000.0) + 5.0,
            )
            rec = {"frame": warmup, "step": checkpoint_key(step)}
            report["recomp_warmup"].append(rec)
            if not step.get("ok"):
                report["error"] = "recomp warmup failed"
                report["first_bad_frame"] = warmup
                report["step_recomp"] = step
                report["dump_recomp"] = collect_dump(recomp, args.window)
                write_report(report_path, report)
                print(f"FAIL oracle recomp warmup frame={warmup}; report={report_path}")
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
                "recomp_rcp": recomp.cmd("cosim_rcp", timeout_s=10.0),
                "recomp_status": recomp.cmd("status", timeout_s=10.0),
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

            if args.cpu_compare == "off":
                cpu_diff = {"ok": True, "different": False, "skipped": True}
            else:
                recomp_regs = recomp.cmd("cosim_regs", timeout_s=10.0)
                ares_cpu = ares.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
                cpu_diff = compare_cpu_int(recomp_regs, ares_cpu)
            rec["cpu_diff"] = cpu_diff

            rdram_diff = {"ok": True, "different": False, "skipped": True}
            if args.rdram_every > 0 and frame % args.rdram_every == 0:
                if args.rdram_search_start > 0:
                    prefix_diff = first_recomp_ares_rdram_diff(
                        recomp,
                        ares,
                        start_offset=0,
                        end_offset=args.rdram_search_start,
                    )
                    if prefix_diff.get("different"):
                        rec["rdram_ignored_prefix_diff"] = {
                            k: v for k, v in prefix_diff.items()
                            if k not in ("recomp", "ares")
                        }
                        report.setdefault("rdram_ignored_prefix_diff", prefix_diff)
                rdram_diff = first_recomp_ares_rdram_diff(
                    recomp,
                    ares,
                    start_offset=args.rdram_search_start,
                    end_offset=args.rdram_search_end,
                )
            rec["rdram_diff"] = {
                k: v for k, v in rdram_diff.items()
                if k not in ("recomp", "ares")
            }

            subdiff = []
            if args.cpu_compare == "fail" and (
                not cpu_diff.get("ok") or cpu_diff.get("different")
            ):
                cpu_subdiff = cpu_diff.get("subdiff")
                if isinstance(cpu_subdiff, list) and cpu_subdiff:
                    subdiff.extend(cpu_subdiff)
                else:
                    subdiff.append("cpu")
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
                if (
                    args.rdram_followup_start > 0
                    and rdram_diff.get("different")
                    and parse_int(rdram_diff.get("paddr", RDRAM_SIZE)) < args.rdram_followup_start
                ):
                    report["rdram_diff_after_followup_start"] = first_recomp_ares_rdram_diff(
                        recomp,
                        ares,
                        start_offset=args.rdram_followup_start,
                    )
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


def run_oracle_align(args: argparse.Namespace) -> int:
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
    if args.recomp_checkpoint < 1:
        raise CosimError("--recomp-checkpoint must be >= 1")
    if args.ares_start < 0 or args.ares_end < args.ares_start:
        raise CosimError("invalid Ares frame sweep range")

    watch_offsets = [rdram_offset(v) for v in args.watch]
    named_ranges = [parse_rdram_range(v) for v in args.ranges]
    recomp = Instance("recomp_align", exe, cwd, args.base_port, log_dir)
    ares = AresInstance("ares_align", ares_exe, rom, ares_cwd, args.base_port + 1, log_dir)
    report: dict[str, Any] = {
        "ok": False,
        "gate": "oracle_alignment_probe",
        "exe": str(exe),
        "cwd": str(cwd),
        "ares_exe": str(ares_exe),
        "rom": str(rom),
        "base_port": args.base_port,
        "recomp_checkpoint_requested": args.recomp_checkpoint,
        "ares_start": args.ares_start,
        "ares_end": args.ares_end,
        "rdram_search_start": args.rdram_search_start,
        "rdram_search_end": args.rdram_search_end,
        "watch": [{"paddr": off, "vaddr": 0x80000000 + off} for off in watch_offsets],
        "ranges": [
            {"name": name, "start": start, "end": end}
            for name, start, end in named_ranges
        ],
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
            print(f"FAIL oracle-align init; report={report_path}")
            return 1

        report["recomp_reset"] = recomp.cmd("cosim_reset")
        report["ares_reset"] = ares.cmd("reset", timeout_s=30.0)
        if not report["recomp_reset"].get("ok") or not report["ares_reset"].get("ok"):
            report["error"] = "reset failed"
            write_report(report_path, report)
            print(f"FAIL oracle-align reset; report={report_path}")
            return 1

        start = recomp.cmd("cosim_start")
        report["recomp_start"] = start
        if not start.get("ok"):
            report["error"] = "recomp cosim_start failed"
            write_report(report_path, report)
            print(f"FAIL oracle-align start; report={report_path}")
            return 1

        report["recomp_steps"] = []
        recomp_step: dict[str, Any] = {}
        for step_index in range(1, args.recomp_checkpoint + 1):
            recomp_step = recomp.cmd(
                {"cmd": "cosim_step", "frames": 1, "timeout_ms": args.step_timeout_ms},
                timeout_s=(args.step_timeout_ms / 1000.0) + 5.0,
            )
            report["recomp_steps"].append({"step": step_index, "checkpoint": checkpoint_key(recomp_step)})
            if not recomp_step.get("ok"):
                report["error"] = "recomp checkpoint step failed"
                report["first_bad_recomp_step"] = step_index
                report["step_recomp"] = recomp_step
                report["dump_recomp"] = collect_dump(recomp, args.window)
                write_report(report_path, report)
                print(f"FAIL oracle-align recomp step={step_index}; report={report_path}")
                return 1
        report["recomp_checkpoint"] = checkpoint_key(recomp_step)

        for warmup in range(1, args.ares_start + 1):
            step = ares.cmd({"cmd": "step_frame", "n": 1}, timeout_s=args.ares_step_timeout)
            if not step.get("ok"):
                report["error"] = "Ares warmup failed"
                report["first_bad_ares_frame"] = warmup
                report["step_ares"] = step
                write_report(report_path, report)
                print(f"FAIL oracle-align Ares warmup frame={warmup}; report={report_path}")
                return 1

        best_row: dict[str, Any] | None = None
        for ares_frame in range(args.ares_start, args.ares_end + 1):
            cpu = ares.cmd({"cmd": "read_cpu_state"}, timeout_s=10.0)
            diff = first_recomp_ares_rdram_diff(
                recomp,
                ares,
                start_offset=args.rdram_search_start,
                end_offset=args.rdram_search_end,
            )
            row: dict[str, Any] = {
                "ares_frame": ares_frame,
                "ares_pc": cpu.get("pc"),
                "cpu_ok": cpu.get("ok"),
                "recomp_rcp": recomp.cmd("cosim_rcp", timeout_s=10.0),
                "recomp_status": recomp.cmd("status", timeout_s=10.0),
                "rdram_diff": summarize_recomp_ares_rdram_diff(diff, include_bytes=args.include_peek_bytes),
            }
            if watch_offsets:
                row["watch"] = [
                    summarize_recomp_ares_rdram_diff(
                        peek_recomp_ares_rdram(recomp, ares, off, args.watch_len),
                        include_bytes=True,
                    )
                    for off in watch_offsets
                ]
            if named_ranges:
                row["range_diffs"] = [
                    {
                        "name": name,
                        **summarize_recomp_ares_rdram_diff(
                            first_recomp_ares_rdram_diff(
                                recomp,
                                ares,
                                start_offset=start,
                                end_offset=end,
                            ),
                            include_bytes=args.include_peek_bytes,
                        ),
                    }
                    for name, start, end in named_ranges
                ]
            report["rows"].append(row)

            if best_row is None:
                best_row = row
            elif not row["rdram_diff"].get("different"):
                best_row = row
            elif best_row["rdram_diff"].get("different") and row["rdram_diff"].get("paddr", -1) > best_row["rdram_diff"].get("paddr", -1):
                best_row = row

            if args.verbose:
                paddr = row["rdram_diff"].get("paddr")
                first = "none" if not row["rdram_diff"].get("different") else f"0x{paddr:06X}"
                print(f"ares_frame={ares_frame} pc={cpu.get('pc')} first_rdram_diff={first}")

            if ares_frame != args.ares_end:
                step = ares.cmd({"cmd": "step_frame", "n": 1}, timeout_s=args.ares_step_timeout)
                if not step.get("ok"):
                    report["error"] = "Ares step failed"
                    report["first_bad_ares_frame"] = ares_frame + 1
                    report["step_ares"] = step
                    write_report(report_path, report)
                    print(f"FAIL oracle-align Ares step frame={ares_frame + 1}; report={report_path}")
                    return 1

        report["ok"] = True
        report["best_by_first_mismatch"] = best_row
        write_report(report_path, report)
        if best_row:
            diff = best_row["rdram_diff"]
            if diff.get("different"):
                print(
                    f"PASS oracle-align recomp_checkpoint={args.recomp_checkpoint} "
                    f"best_ares_frame={best_row.get('ares_frame')} first_rdram_diff=0x{diff.get('paddr', 0):06X}; "
                    f"report={report_path}"
                )
            else:
                print(
                    f"PASS oracle-align recomp_checkpoint={args.recomp_checkpoint} "
                    f"best_ares_frame={best_row.get('ares_frame')} first_rdram_diff=none; report={report_path}"
                )
        else:
            print(f"PASS oracle-align no rows; report={report_path}")
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

    ares_gate = sub.add_parser("ares-gate", help="run Ares-vs-Ares oracle determinism gate")
    ares_gate.add_argument("--frames", type=int, default=20)
    ares_gate.add_argument("--base-port", type=int, default=4874)
    ares_gate.add_argument("--startup-timeout", type=float, default=30.0)
    ares_gate.add_argument("--step-timeout", type=float, default=90.0)
    ares_gate.add_argument("--rdram-every", type=int, default=1, help="compare full RDRAM every N frames; 0 disables")
    ares_gate.add_argument("--ares-exe", default=str(DEFAULT_ARES_EXE))
    ares_gate.add_argument("--ares-cwd", default=str(DEFAULT_ARES_EXE.parent))
    ares_gate.add_argument("--rom", default=str(DEFAULT_ROM))
    ares_gate.add_argument("--log-dir", default=str(BUILD / "cosim_ares_gate_logs"))
    ares_gate.add_argument("--report", default=str(BUILD / "cosim_ares_gate_report.json"))
    ares_gate.add_argument("--verbose", action="store_true")

    oracle = sub.add_parser("oracle", help="run recomp-vs-Ares per-VI checkpoint diff")
    oracle.add_argument("--frames", type=int, default=60)
    oracle.add_argument("--step-timeout-ms", type=int, default=15000)
    oracle.add_argument("--ares-step-timeout", type=float, default=90.0)
    oracle.add_argument("--ares-warmup-frames", type=int, default=13, help="step Ares this many VI frames before starting recomp")
    oracle.add_argument("--recomp-warmup-frames", type=int, default=0, help="step recomp this many VI checkpoints after cosim_start before comparisons")
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
    oracle.add_argument(
        "--rdram-search-start",
        type=lambda x: int(x, 0),
        default=0,
        help="first RDRAM paddr included in the gating diff; use 0x400 to skip the raw IPL/HLE boot aperture",
    )
    oracle.add_argument(
        "--rdram-search-end",
        type=lambda x: int(x, 0),
        default=RDRAM_SIZE,
        help="exclusive RDRAM paddr end for the gating diff",
    )
    oracle.add_argument(
        "--rdram-followup-start",
        type=lambda x: int(x, 0),
        default=0x100000,
        help="after an earlier RDRAM mismatch, also report the first mismatch at/after this paddr; 0 disables",
    )
    oracle.add_argument(
        "--cpu-compare",
        choices=("fail", "report", "off"),
        default="fail",
        help="whether CPU GPR/HI/LO differences gate the oracle; report keeps them in the report while RDRAM alignment is refined",
    )
    oracle.add_argument("--verbose", action="store_true")

    align = sub.add_parser("oracle-align", help="sweep Ares VI frames against one recomp checkpoint")
    align.add_argument("--recomp-checkpoint", type=int, default=1, help="recomp VI checkpoints to step after cosim_start before holding state")
    align.add_argument("--ares-start", type=int, default=50, help="first completed Ares VI frame to compare")
    align.add_argument("--ares-end", type=int, default=70, help="last completed Ares VI frame to compare")
    align.add_argument("--step-timeout-ms", type=int, default=15000)
    align.add_argument("--ares-step-timeout", type=float, default=90.0)
    align.add_argument("--startup-timeout", type=float, default=30.0)
    align.add_argument("--base-port", type=int, default=4910)
    align.add_argument("--exe", default=str(DEFAULT_EXE))
    align.add_argument("--cwd", default=str(BUILD))
    align.add_argument("--ares-exe", default=str(DEFAULT_ARES_EXE))
    align.add_argument("--ares-cwd", default=str(DEFAULT_ARES_EXE.parent))
    align.add_argument("--rom", default=str(DEFAULT_ROM))
    align.add_argument("--log-dir", default=str(BUILD / "cosim_oracle_align_logs"))
    align.add_argument("--report", default=str(BUILD / "cosim_oracle_align_report.json"))
    align.add_argument("--window", type=int, default=16)
    align.add_argument(
        "--rdram-search-start",
        type=lambda x: int(x, 0),
        default=0x400,
        help="first RDRAM paddr included in the probe diff",
    )
    align.add_argument(
        "--rdram-search-end",
        type=lambda x: int(x, 0),
        default=RDRAM_SIZE,
        help="exclusive RDRAM paddr end for the probe diff",
    )
    align.add_argument(
        "--watch",
        action="append",
        default=[],
        help="RDRAM paddr or KSEG0/KSEG1 vaddr to record at every compared Ares frame",
    )
    align.add_argument(
        "--range",
        dest="ranges",
        action="append",
        default=[],
        help="named RDRAM range to diff at every row: name:start:end",
    )
    align.add_argument("--watch-len", type=int, default=4)
    align.add_argument("--include-peek-bytes", action="store_true")
    align.add_argument("--verbose", action="store_true")

    args = ap.parse_args(argv)
    if args.cmd == "gate1":
        return run_gate(args)
    if args.cmd == "gate3":
        return run_gate3(args)
    if args.cmd == "ares-smoke":
        return run_ares_smoke(args)
    if args.cmd == "ares-gate":
        return run_ares_gate(args)
    if args.cmd == "oracle":
        return run_oracle(args)
    if args.cmd == "oracle-align":
        return run_oracle_align(args)
    raise AssertionError(args.cmd)


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except CosimError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(2)
