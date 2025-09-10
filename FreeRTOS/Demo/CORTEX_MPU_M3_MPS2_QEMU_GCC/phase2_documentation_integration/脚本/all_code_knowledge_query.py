#!/usr/bin/env python3
# -*- coding: utf-8 -*-
from __future__ import annotations
import csv
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional
import argparse

BASE_DIR = Path(__file__).resolve().parent
# 数据目录在上一级
DATA_DIR = BASE_DIR.parent / "graph_export_analysis"


def _load_json(path: Path) -> Any:
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def _load_csv(path: Path) -> List[Dict[str, str]]:
    with open(path, "r", encoding="utf-8") as f:
        return list(csv.DictReader(f))


@dataclass
class Function:
    id: str
    name: str
    fullName: str
    signature: str
    filename: str
    start_line: int
    end_line: int
    column: int
    isExternal: bool
    numberOfLines: int
    cyclomaticComplexity: int
    parameters: List[Dict[str, Any]]
    source_code: str


class KnowledgeBase:
    def __init__(self, data_dir: Path = DATA_DIR):
        self.data_dir = Path(data_dir)
        if not self.data_dir.exists():
            raise FileNotFoundError(f"Data dir not found: {self.data_dir}")
        # load
        self.functions: List[Function] = []
        for rec in _load_json(self.data_dir / "functions_detailed.json"):
            self.functions.append(Function(
                id=str(rec.get("id", "")),
                name=rec.get("name", ""),
                fullName=rec.get("fullName", ""),
                signature=rec.get("signature", ""),
                filename=rec.get("filename", ""),
                start_line=int(rec.get("start_line", -1) or -1),
                end_line=int(rec.get("end_line", -1) or -1),
                column=int(rec.get("column", -1) or -1),
                isExternal=bool(rec.get("isExternal", False)),
                numberOfLines=int(rec.get("numberOfLines", -1) or -1),
                cyclomaticComplexity=int(rec.get("cyclomaticComplexity", -1) or -1),
                parameters=list(rec.get("parameters", [])),
                source_code=rec.get("source_code", ""),
            ))
        self.calls: List[Dict[str, Any]] = _load_csv(self.data_dir / "function_calls_resolved.csv")
        self.types: List[Dict[str, Any]] = _load_csv(self.data_dir / "types.csv")
        self.members: List[Dict[str, Any]] = _load_csv(self.data_dir / "members.csv")
        self.files: List[Dict[str, Any]] = _load_csv(self.data_dir / "files.csv")

        # indexes
        self._funcs_by_name: Dict[str, List[Function]] = {}
        self._funcs_by_file: Dict[str, List[Function]] = {}
        for f in self.functions:
            self._funcs_by_name.setdefault(f.name, []).append(f)
            self._funcs_by_file.setdefault(f.filename, []).append(f)
        for lst in self._funcs_by_file.values():
            lst.sort(key=lambda x: (x.start_line, x.end_line))

    # --------- Function lookups ---------
    def get_function_by_name(self, name: str) -> List[Function]:
        return list(self._funcs_by_name.get(name, []))

    def get_function_by_fullname(self, full: str) -> List[Function]:
        return [f for f in self.functions if f.fullName == full]

    def get_functions_in_file(self, file_path: str) -> List[Function]:
        return list(self._funcs_by_file.get(file_path, []))

    def get_function_at(self, file_path: str, line: int) -> Optional[Function]:
        for f in self._funcs_by_file.get(file_path, []):
            if f.start_line <= line <= f.end_line:
                return f
        return None

    def get_function_source(self, name: str) -> str:
        lst = self.get_function_by_name(name)
        return lst[0].source_code if lst else ""

    # --------- Calls ---------
    def get_calls_by_caller(self, caller_name: str) -> List[Dict[str, Any]]:
        return [c for c in self.calls if c.get("caller_name") == caller_name]

    def get_calls_to(self, callee_name: str) -> List[Dict[str, Any]]:
        return [c for c in self.calls if c.get("callee") == callee_name]

    def get_outgoing_callees(self, caller_name: str) -> List[str]:
        return sorted({c.get("callee", "") for c in self.get_calls_by_caller(caller_name) if c.get("callee")})

    def get_incoming_callers(self, callee_name: str) -> List[str]:
        return sorted({c.get("caller_name", "") for c in self.get_calls_to(callee_name) if c.get("caller_name")})

    # --------- Types / Members ---------
    def get_types(self, kind: Optional[str] = None, name_contains: Optional[str] = None) -> List[Dict[str, Any]]:
        out = self.types
        if kind:
            out = [t for t in out if t.get("kind") == kind]
        if name_contains:
            out = [t for t in out if name_contains in (t.get("name") or "")]
        return out

    def get_members_of_type(self, type_name: str) -> List[Dict[str, Any]]:
        out: List[Dict[str, Any]] = []
        for m in self.members:
            decl_in = m.get("decl_in") or ""
            if decl_in.endswith(type_name) or decl_in == type_name:
                out.append(m)
        return out


def _print_func(f: Function):
    params = ", ".join(f"{p.get('type','')} {p.get('name','')}".strip() for p in f.parameters)
    print(f"{f.name} {f.signature} @ {f.filename}:{f.start_line}-{f.end_line}")
    if params:
        print(f"  params: {params}")


def main():
    ap = argparse.ArgumentParser(description="Query all-code knowledge base")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--function", help="Show function details by name")
    g.add_argument("--calls-from", dest="calls_from", help="List calls made by function name")
    g.add_argument("--calls-to", dest="calls_to", help="List call sites to a callee name")
    g.add_argument("--type", dest="type_name", help="Search types by name substring")
    ap.add_argument("--kind", choices=["struct", "union", "enum", "type"], help="Type kind filter")
    args = ap.parse_args()

    kb = KnowledgeBase()
    if args.function:
        funcs = kb.get_function_by_name(args.function)
        if not funcs:
            print("No function found")
            return
        for f in funcs:
            _print_func(f)
            print("  outgoing callees:", ", ".join(kb.get_outgoing_callees(f.name)))
    elif args.calls_from:
        edges = kb.get_calls_by_caller(args.calls_from)
        for e in edges:
            print(f"{e.get('caller_name')}:{e.get('call_line')} -> {e.get('callee')}")
    elif args.calls_to:
        edges = kb.get_calls_to(args.calls_to)
        for e in edges:
            print(f"{e.get('caller_name')}:{e.get('call_line')} -> {e.get('callee')}")
    elif args.type_name:
        types = kb.get_types(kind=args.kind, name_contains=args.type_name)
        for t in types:
            print(f"{t.get('kind')} {t.get('name')} @ {t.get('filename')}:{t.get('start_line')}-{t.get('end_line')}")


if __name__ == "__main__":
    main()
