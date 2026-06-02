"""Inspect the quick-battle / audio dispatch crash post-mortem.

Surfaces SEH faulting state, audio ring alignment, the BAD probe values
that fired, and which thread was running at crash time.
"""
import json
import sys


def main() -> int:
    p = sys.argv[1] if len(sys.argv) > 1 else "build/last_run_report.json"
    d = json.load(open(p))

    print("=== reason / SEH ===")
    print(f"  reason: {d.get('reason')}")
    for k, v in d.get("seh", {}).items():
        print(f"  seh.{k}: {v}")

    print("\n=== status counters ===")
    for k, v in d.get("status", {}).items():
        print(f"  {k}: {v}")

    print("\n=== audio ring totals ===")
    for r in ["audio_afx_in", "audio_afx_out", "audio_mbp_pre", "audio_mbp_disp"]:
        e = d.get(r, {})
        print(f"  {r}: count={e.get('count')} total_seq={e.get('total_seq')}")

    print("\n=== audio rings — last 8 entries each ===")
    for r in ["audio_afx_in", "audio_afx_out", "audio_mbp_pre", "audio_mbp_disp"]:
        entries = d.get(r, {}).get("entries", [])
        print(f"  {r}:")
        for e in entries[-8:]:
            print(f"    {e}")

    print("\n=== threads (faulting first) ===")
    fault_pc = d.get("seh", {}).get("pc_hex") or d.get("seh", {}).get("pc")
    for t in d.get("threads", []):
        name = t.get("name", "?")
        tid = t.get("tid", "?")
        st = t.get("state", "?")
        top = (t.get("stack") or [{}])[0].get("symbol", "?")
        is_fault = "*FAULT*" if any(
            (frame.get("pc_hex") == fault_pc or frame.get("pc") == fault_pc)
            for frame in (t.get("stack") or [])
        ) else ""
        print(f"  tid={tid} name={name!r:30s} state={st:12s} top={top} {is_fault}")


if __name__ == "__main__":
    raise SystemExit(main())
