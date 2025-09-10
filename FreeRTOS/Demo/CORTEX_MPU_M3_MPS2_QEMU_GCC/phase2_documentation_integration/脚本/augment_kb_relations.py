#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import annotations
import csv
import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple, Any, Optional

BASE_DIR = Path(__file__).resolve().parent
DATA_DIR = BASE_DIR.parent / "graph_export_analysis"
WORKSPACE_ROOT = BASE_DIR.parent.parent.parent.parent  # /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC


def load_csv(p: Path) -> List[Dict[str, str]]:
    with open(p, "r", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def load_json(p: Path) -> Any:
    with open(p, "r", encoding="utf-8") as f:
        return json.load(f)


def write_csv(p: Path, rows: List[Dict[str, Any]]):
    if not rows:
        return
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def write_json(p: Path, data: Any):
    p.parent.mkdir(parents=True, exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)


# ------------- include 解析 -------------
INCLUDE_RE = re.compile(r"^\s*#\s*include\s*([<]\s*[^>]+\s*[>]|[\"]\s*[^\"]+\s*[\"])\s*")


def find_compile_commands() -> Optional[Path]:
    # 向上查找 compile_commands.json
    cur = BASE_DIR
    for _ in range(6):
        candidate = cur / "compile_commands.json"
        if candidate.exists():
            return candidate
        cur = cur.parent
    return None


def extract_include_paths(cc_path: Path) -> List[Path]:
    paths: List[Path] = []
    try:
        entries = json.loads(cc_path.read_text(encoding="utf-8"))
    except Exception:
        return []
    for e in entries:
        cmd = e.get("command") or e.get("arguments") or ""
        if isinstance(cmd, list):
            parts = cmd
        else:
            # naive split is ok for -I scanning
            parts = str(cmd).split()
        for i, tok in enumerate(parts):
            if tok.startswith("-I"):
                val = tok[2:] or (parts[i + 1] if i + 1 < len(parts) else "")
                if val:
                    p = Path(val)
                    if not p.is_absolute():
                        # base is file directory if provided
                        base = Path(e.get("directory") or cc_path.parent)
                        p = (base / p).resolve()
                    if p.exists():
                        paths.append(p)
    # de-dup while preserving order
    seen = set()
    uniq: List[Path] = []
    for p in paths:
        if p not in seen:
            uniq.append(p)
            seen.add(p)
    return uniq


def parse_includes_for_file(src_path: Path, angle_search: List[Path]) -> List[Tuple[str, str]]:
    res: List[Tuple[str, str]] = []
    try:
        lines = src_path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except Exception:
        return res
    for ln in lines:
        m = INCLUDE_RE.match(ln)
        if not m:
            continue
        raw = m.group(1).strip()
        if raw.startswith("<") and raw.endswith(">"):
            header = raw[1:-1].strip()
            # resolve via -I search
            resolved = None
            for base in angle_search:
                cand = (base / header).resolve()
                if cand.exists():
                    resolved = str(cand)
                    break
            res.append((header, resolved or ""))
        elif raw.startswith('"') and raw.endswith('"'):
            header = raw[1:-1].strip()
            cand = (src_path.parent / header).resolve()
            res.append((header, str(cand) if cand.exists() else ""))
    return res


# ------------- 调用关系增强 -------------
OPERATOR_PREFIXES = (
    "operator ", "<operator>", "operator:",
)


def classify_callee(name: str) -> str:
    n = name or ""
    if not n:
        return "unknown"
    low = n.lower()
    if low.startswith(OPERATOR_PREFIXES) or n.startswith("::operator"):
        return "operator"
    if ":ANY(" in n or n.endswith(") [macro]"):
        return "macro"
    return "function"


def build_func_index(functions: List[Dict[str, Any]]) -> Dict[str, List[Dict[str, Any]]]:
    idx: Dict[str, List[Dict[str, Any]]] = {}
    for f in functions:
        idx.setdefault(f.get("name") or "", []).append(f)
    return idx


def augment():
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    functions = load_json(DATA_DIR / "functions_detailed.json")
    file_table = load_csv(DATA_DIR / "files.csv")
    calls_raw_path = DATA_DIR / "function_calls.csv"
    calls_resolved_path = DATA_DIR / "function_calls_resolved.csv"
    includes_parsed_path = DATA_DIR / "includes_parsed.csv"
    summary_path = DATA_DIR / "augment_summary.json"

    calls = load_csv(calls_raw_path)
    func_index = build_func_index(functions)

    # include search paths
    ccjson = find_compile_commands()
    angle_paths = extract_include_paths(ccjson) if ccjson else []

    # build file->abs path mapping
    file_abs: Dict[str, Path] = {}
    for row in file_table:
        raw = row.get("path") or row.get("filename") or row.get("name") or ""
        p = Path(raw)
        if not p.is_absolute():
            p = (WORKSPACE_ROOT / p).resolve()
        file_abs[raw or str(p)] = p

    # parse includes
    includes_rows: List[Dict[str, Any]] = []
    for key, p in file_abs.items():
        if not p.exists() or p.is_dir():
            continue
        incs = parse_includes_for_file(p, angle_paths)
        for header, resolved in incs:
            includes_rows.append({
                "file": str(p),
                "include": header,
                "resolved": resolved,
            })

    # resolve callees
    resolved_rows: List[Dict[str, Any]] = []
    stats = {
        "calls_total": 0,
        "calls_resolved_ok": 0,
        "calls_resolved_multi": 0,
        "calls_unresolved": 0,
        "includes_parsed": len(includes_rows),
    }

    for c in calls:
        stats["calls_total"] += 1
        callee = c.get("callee") or ""
        kind = classify_callee(callee)
        rec = dict(c)
        rec["callee_kind"] = kind
        rec["resolved_defs"] = ""

        if kind != "function":
            resolved_rows.append(rec)
            continue

        defs = func_index.get(callee) or []
        if not defs:
            stats["calls_unresolved"] += 1
            resolved_rows.append(rec)
            continue

        if len(defs) == 1:
            d = defs[0]
            rec["resolved_defs"] = f"{d.get('filename')}:{d.get('start_line')}-{d.get('end_line')}"
            stats["calls_resolved_ok"] += 1
        else:
            # 多重定义，全部列出
            rec["resolved_defs"] = ";".join(
                f"{d.get('filename')}:{d.get('start_line')}-{d.get('end_line')}" for d in defs
            )
            stats["calls_resolved_multi"] += 1
        resolved_rows.append(rec)

    # write outputs
    write_csv(includes_parsed_path, includes_rows)
    write_csv(calls_resolved_path, resolved_rows)
    write_json(summary_path, stats)

    print(json.dumps(stats, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    augment()
