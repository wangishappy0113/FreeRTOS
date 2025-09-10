#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Parse Joern GraphSON export.json and emit complete CSV/JSON for:
- Functions (metadata + full source code + call relations + file positions)
- Types / Structs / Members

Input:
  /home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC/with_compdb_cpg.json/export.json
Output dir:
  phase2_documentation_integration/graph_export_analysis/

Notes:
- Works with GraphSON from `joern-export --repr all --format graphson`.
- Handles property shapes of GraphSON v2/v3 (list of props vs flat props).
- Reconstruct function code by slicing local source file using [start_line, end_line].
- Builds call relations by climbing AST parent pointers from CALL nodes to owning METHOD.
"""

from __future__ import annotations
import json
import os
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Iterable
import sys

BASE_DIR = Path(__file__).resolve().parent
WORK_DIR = BASE_DIR
EXPORT_JSON = Path("/home/zwz/FreeRTOS/FreeRTOS/Demo/CORTEX_MPU_M3_MPS2_QEMU_GCC/with_compdb_cpg.json/export.json")
OUT_DIR = WORK_DIR / "graph_export_analysis"
OUT_DIR.mkdir(exist_ok=True)

# -------------------------- Helpers --------------------------

def norm_key(name: str) -> str:
    return name.lower().replace(" ", "").replace("_", "")

PROP_ALIASES = {
    "name": {"name"},
    "fullname": {"fullname", "full_name", "fullName"},
    "signature": {"signature"},
    "filename": {"filename", "file", "filepath"},
    "linenumber": {"linenumber", "line", "startline"},
    "linenumberend": {"linenumberend", "endline"},
    "columnnumber": {"columnnumber", "column"},
    "columnnumberend": {"columnnumberend", "columnend"},
    "isexternal": {"isexternal", "external"},
    "code": {"code"},
    "numberoflines": {"numberoflines"},
    "cyclomaticcomplexity": {"cyclomaticcomplexity", "complexity"},
    "typefullname": {"typefullname", "type"},
    "order": {"order"},
}


def _unwrap_value(v: Any) -> Any:
    """Unwrap common GraphSON and container shapes into plain Python values.
    - Dicts with '@value' -> that value
    - Lists: if single element, return that; else return list as-is
    - For Dicts like {'id':..., 'value': ...} -> return 'value'
    """
    if isinstance(v, dict):
        # GraphSON typed value
        if "@value" in v and (len(v) == 1 or (len(v) == 2 and "id" in v)):
            return _unwrap_value(v.get("@value"))
        # Common {value: x}
        if "value" in v and len(v) in (1, 2):
            return _unwrap_value(v.get("value"))
        # Recursively unwrap nested dicts
        return {k: _unwrap_value(val) for k, val in v.items() if k != "@type"}
    if isinstance(v, list):
        if len(v) == 0:
            return v
        if len(v) == 1:
            return _unwrap_value(v[0])
        return [_unwrap_value(it) for it in v]
    return v


def _to_bool(v: Any) -> bool:
    if isinstance(v, bool):
        return v
    if isinstance(v, (int, float)):
        return bool(v)
    if isinstance(v, str):
        return v.strip().lower() in {"1", "true", "yes"}
    return bool(v)


def get_prop(obj: Dict[str, Any], key: str) -> Any:
    # GraphSON may keep properties under obj["properties"][key][0]["value"], or directly under obj[key]
    aliases = PROP_ALIASES.get(norm_key(key), {key})
    # 1) direct
    for k in list(aliases) + [key]:
        if k in obj:
            return _unwrap_value(obj[k])
    # 2) properties map
    props = obj.get("properties") or {}
    for k in props.keys():
        if norm_key(k) in {norm_key(a) for a in aliases}:
            v = props[k]
            # v can be list of {id, value}
            if isinstance(v, list) and v:
                return _unwrap_value(v)
            # or a dict {key:..., value:...}
            if isinstance(v, dict):
                return _unwrap_value(v)
            return _unwrap_value(v)
    # 3) meta properties
    for k in obj.keys():
        if norm_key(k) in {norm_key(a) for a in aliases}:
            v = obj[k]
            if isinstance(v, dict):
                if "@value" in v:
                    return _unwrap_value(v["@value"]) 
                if "value" in v:
                    return _unwrap_value(v["value"]) 
            return _unwrap_value(v)
    return None


def get_prop_int(obj: Dict[str, Any], key: str, default: int = -1) -> int:
    v = get_prop(obj, key)
    try:
        if v is None:
            return default
        return int(v)
    except Exception:
        return default


def safe_read_file_slice(path: str, start_line: int, end_line: int) -> str:
    if not path or start_line <= 0 or end_line < start_line:
        return ""
    try:
        with open(path, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
        start = max(1, start_line)
        end = min(len(lines), end_line)
        if start > end:
            return ""
        return "".join(lines[start-1:end])
    except Exception:
        return ""

# ---------------------- Parsing GraphSON ----------------------

def _unwrap_graphson(obj: Any) -> Any:
    """Recursively unwrap GraphSON v3 objects to plain Python types."""
    if isinstance(obj, dict):
        # Prefer GraphSON '@value'
        if "@value" in obj and len(obj) <= 2:
            return _unwrap_graphson(obj["@value"])
        # Otherwise unwrap each field
        return {k: _unwrap_graphson(v) for k, v in obj.items() if k != "@type"}
    if isinstance(obj, list):
        return [_unwrap_graphson(it) for it in obj]
    return obj


def load_graphson(path: Path) -> Tuple[Dict[str, Dict], List[Dict]]:
    print(f"[+] Loading GraphSON: {path}")
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        raw = json.load(f)
    data = _unwrap_graphson(raw)
    # Accept shapes: {graph:{vertices,edges}} or {@value:{vertices,edges}} already unwrapped
    graph = data.get("graph", data)
    # Some GraphSON dumps put vertices/edges under graph -> {@value} which is now unwrapped
    vertices = graph.get("vertices") or []
    edges = graph.get("edges") or []
    # Normalize edge endpoint ids
    for e in edges:
        if isinstance(e.get("outV"), dict) and "id" in e["outV"]:
            # In some dumps edge endpoints may be nested vertex refs; unwrap to id
            e["outV"] = e["outV"].get("id")
        if isinstance(e.get("inV"), dict) and "id" in e["inV"]:
            e["inV"] = e["inV"].get("id")
    vmap: Dict[str, Dict] = {}
    for v in vertices:
        # Fully unwrap any residual GraphSON wrappers in each vertex
        v_unwrapped = _unwrap_graphson(v)
        vid = v_unwrapped.get("id")
        vmap[str(vid)] = v_unwrapped
    print(f"[+] Vertices: {len(vertices)} | Edges: {len(edges)}")
    return vmap, edges

# ---------------------- Build Indexes -------------------------

def build_indexes(vmap: Dict[str, Dict], edges: List[Dict]) -> Dict[str, Any]:
    print("[+] Building indexes...")
    # child -> parent via AST (parent is outV, child is inV)
    parent_of: Dict[str, str] = {}
    ast_children_of: Dict[str, List[str]] = {}
    incoming_by_label: Dict[str, Dict[str, List[str]]] = {}
    outgoing_by_label: Dict[str, Dict[str, List[str]]] = {}

    def add_edge_index(d: Dict[str, Dict[str, List[str]]], label: str, src: str, dst: str):
        if label not in d:
            d[label] = {}
        d[label].setdefault(src, []).append(dst)

    for e in edges:
        label = e.get("label") or e.get("type") or ""
        outv = str(e.get("outV")) if e.get("outV") is not None else None
        inv = str(e.get("inV")) if e.get("inV") is not None else None
        if not outv or not inv:
            continue
        # parent/child map for AST
        if label.upper() == "AST":
            parent_of[inv] = outv
            ast_children_of.setdefault(outv, []).append(inv)
        # generic indexes
        add_edge_index(outgoing_by_label, label, outv, inv)
        add_edge_index(incoming_by_label, label, inv, outv)

    print(f"[+] AST parent map: {len(parent_of)} nodes with parents")
    return {
        "parent_of": parent_of,
        "ast_children_of": ast_children_of,
        "incoming_by_label": incoming_by_label,
        "outgoing_by_label": outgoing_by_label,
    }

# -------------------- Extract Functions ----------------------

def extract_functions(vmap: Dict[str, Dict]) -> List[Tuple[str, Dict]]:
    funcs: List[Tuple[str, Dict]] = []
    for vid, v in vmap.items():
        label = (v.get("label") or "").upper()
        if label == "METHOD":
            funcs.append((vid, v))
    print(f"[+] METHODS: {len(funcs)}")
    return funcs


def nearest_method(vid: str, parent_of: Dict[str, str], vmap: Dict[str, Dict]) -> Optional[str]:
    cur = vid
    visited = 0
    while visited < 8192 and cur in parent_of:
        parent = parent_of[cur]
        plabel = (vmap.get(parent, {}).get("label") or "").upper()
        if plabel == "METHOD":
            return parent
        cur = parent
        visited += 1
    # Fallback: if current has a parent but not found METHOD, return last parent
    return parent_of.get(cur)


def extract_calls(vmap: Dict[str, Dict], idx: Dict[str, Any]) -> List[Dict]:
    parent_of = idx["parent_of"]
    calls: List[Dict] = []
    for vid, v in vmap.items():
        if (v.get("label") or "").upper() != "CALL":
            continue
        callee_name = (
            get_prop(v, "methodFullName")
            or get_prop(v, "METHOD_FULL_NAME")
            or get_prop(v, "name")
            or ""
        )
        # Try multiple aliases for line number and filename
        call_line = get_prop_int(v, "lineNumber", -1)
        if call_line == -1:
            call_line = get_prop_int(v, "line", -1)
        call_file = get_prop(v, "filename") or get_prop(v, "file") or ""
        owner_mid = nearest_method(vid, parent_of, vmap) or ""
        owner = vmap.get(owner_mid) if owner_mid else None
        caller_name = get_prop(owner or {}, "name") or ""
        caller_full = get_prop(owner or {}, "fullName") or ""
        caller_file = get_prop(owner or {}, "filename") or ""
        if not call_file:
            call_file = caller_file
        calls.append({
            "caller_name": caller_name,
            "caller_fullName": caller_full,
            "caller_file": caller_file,
            "call_line": call_line,
            "callee": callee_name,
            "call_file": call_file,
        })
    print(f"[+] CALL sites: {len(calls)}")
    return calls


def extract_parameters(vmap: Dict[str, Dict], idx: Dict[str, Any]) -> Dict[str, List[Dict]]:
    ast_children_of = idx["ast_children_of"]
    params_by_method: Dict[str, List[Dict]] = {}
    for m_id, children in ast_children_of.items():
        m = vmap.get(m_id)
        if not m or (m.get("label") or "").upper() != "METHOD":
            continue
        out: List[Dict] = []
        for c_id in children:
            node = vmap.get(c_id)
            if not node:
                continue
            if (node.get("label") or "").upper() in {"METHOD_PARAMETER_IN", "METHOD_PARAMETER"}:
                out.append({
                    "name": get_prop(node, "name") or "",
                    "type": get_prop(node, "typeFullName") or "",
                    "order": get_prop_int(node, "order", 0),
                })
        if out:
            out.sort(key=lambda x: x.get("order", 0))
            params_by_method[m_id] = out
    print(f"[+] Methods with parameters: {len(params_by_method)}")
    return params_by_method

# -------------------- Extract Files & Includes -------------------

def extract_files(vmap: Dict[str, Dict]) -> Dict[str, Dict]:
    files: Dict[str, Dict] = {}
    for vid, v in vmap.items():
        if (v.get("label") or "").upper() == "FILE":
            name = get_prop(v, "name") or get_prop(v, "filename") or ""
            files[vid] = {
                "id": vid,
                "name": name,
            }
    print(f"[+] FILE vertices: {len(files)}")
    return files


def extract_includes(edges: List[Dict], files: Dict[str, Dict]) -> List[Dict]:
    includes: List[Dict] = []
    for e in edges:
        label = (e.get("label") or e.get("type") or "").upper()
        if label != "INCLUDE":
            continue
        src = str(e.get("outV"))
        dst = str(e.get("inV"))
        src_name = files.get(src, {}).get("name", "")
        dst_name = files.get(dst, {}).get("name", "")
        includes.append({
            "source_file": src_name,
            "target_file": dst_name,
        })
    print(f"[+] INCLUDE edges: {len(includes)}")
    return includes

# -------------------- Extract Types/Structs -------------------

def extract_types(vmap: Dict[str, Dict], idx: Dict[str, Any]) -> Tuple[List[Dict], List[Dict]]:
    types: List[Dict] = []
    members: List[Dict] = []
    for vid, v in vmap.items():
        label = (v.get("label") or "").upper()
        if label == "TYPE_DECL":
            code_raw = get_prop(v, "code")
            code = code_raw if isinstance(code_raw, str) else (json.dumps(code_raw, ensure_ascii=False) if code_raw is not None else "")
            name_raw = get_prop(v, "name")
            name = name_raw if isinstance(name_raw, str) else (str(name_raw) if name_raw is not None else "")
            kind = "unknown"
            head = code.strip().lower()
            if head.startswith("struct ") or name.startswith("struct "):
                kind = "struct"
            elif head.startswith("union ") or name.startswith("union "):
                kind = "union"
            elif head.startswith("enum ") or name.startswith("enum "):
                kind = "enum"
            else:
                kind = "type"  # could be C++ class or typedef-like decl
            types.append({
                "name": name,
                "fullName": get_prop(v, "fullName") or "",
                "filename": get_prop(v, "filename") or "",
                "start_line": get_prop_int(v, "lineNumber", -1),
                "end_line": get_prop_int(v, "lineNumberEnd", -1),
                "code": code,
                "kind": kind,
            })
        elif label == "MEMBER":
            members.append({
                "name": get_prop(v, "name") or "",
                "typeFullName": get_prop(v, "typeFullName") or "",
                "filename": get_prop(v, "filename") or "",
                "line": get_prop_int(v, "lineNumber", -1),
                "decl_in": get_prop(v, "definedByTypeFullName") or "",
            })
    print(f"[+] TYPE_DECL: {len(types)} | MEMBER: {len(members)}")
    return types, members

# -------------------- Glue: Build Function Records ------------

def build_function_records(funcs: List[Tuple[str, Dict]], params_by_m: Dict[str, List[Dict]]) -> List[Dict]:
    results: List[Dict] = []
    for mid, mv in funcs:
        filename = get_prop(mv, "filename") or get_prop(mv, "file") or ""
        start_line = get_prop_int(mv, "lineNumber", -1)
        if start_line == -1:
            start_line = get_prop_int(mv, "line", -1)
        end_line = get_prop_int(mv, "lineNumberEnd", -1)
        if end_line == -1:
            end_line = get_prop_int(mv, "endLine", -1)
        code_slice = safe_read_file_slice(filename, start_line, end_line)
        rec = {
            "id": mid,
            "name": get_prop(mv, "name") or "",
            "fullName": get_prop(mv, "fullName") or "",
            "signature": get_prop(mv, "signature") or "",
            "filename": filename,
            "start_line": start_line,
            "end_line": end_line,
            "column": get_prop_int(mv, "columnNumber", -1),
            "isExternal": _to_bool(get_prop(mv, "isExternal")),
            "numberOfLines": get_prop_int(mv, "numberOfLines", -1),
            "cyclomaticComplexity": get_prop_int(mv, "cyclomaticComplexity", -1),
            "parameters": params_by_m.get(mid, []),
            "source_code": code_slice,
        }
        results.append(rec)
    print(f"[+] Built function records: {len(results)}")
    return results

# -------------------- Write CSV/JSON --------------------------

def write_json(path: Path, data: Any):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)


def write_csv(path: Path, rows: List[Dict], headers: List[str]):
    import csv
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=headers)
        w.writeheader()
        for r in rows:
            w.writerow({k: r.get(k, "") for k in headers})

# -------------------- Main --------------------------

def main():
    # 1) Load GraphSON
    vmap, edges = load_graphson(EXPORT_JSON)
    idx = build_indexes(vmap, edges)

    # 2) Extract functions, parameters, calls
    funcs = extract_functions(vmap)
    params_by_m = extract_parameters(vmap, idx)
    calls = extract_calls(vmap, idx)

    func_records = build_function_records(funcs, params_by_m)

    # 3) Extract types/structs/members
    types, members = extract_types(vmap, idx)

    # 4) Write outputs
    write_json(OUT_DIR / "functions_detailed.json", func_records)
    write_csv(
        OUT_DIR / "functions_summary.csv",
        [
            {
                "name": r["name"],
                "fullName": r["fullName"],
                "signature": r["signature"],
                "filename": r["filename"],
                "start_line": r["start_line"],
                "end_line": r["end_line"],
                "column": r["column"],
                "cyclomaticComplexity": r["cyclomaticComplexity"],
                "numberOfLines": r["numberOfLines"],
                "parameters": ", ".join(f"{p.get('type','')} {p.get('name','')}".strip() for p in r.get("parameters", [])),
            }
            for r in func_records
        ],
        headers=[
            "name", "fullName", "signature", "filename", "start_line", "end_line", "column",
            "cyclomaticComplexity", "numberOfLines", "parameters",
        ],
    )

    write_csv(
        OUT_DIR / "function_calls.csv",
        calls,
        headers=[
            "caller_name", "caller_fullName", "caller_file", "call_line", "callee", "call_file",
        ],
    )

    write_csv(
        OUT_DIR / "types.csv",
        types,
        headers=["name", "fullName", "filename", "start_line", "end_line", "kind", "code"],
    )

    write_csv(
        OUT_DIR / "members.csv",
        members,
        headers=["name", "typeFullName", "decl_in", "filename", "line"],
    )

    # Files and includes
    files = extract_files(vmap)
    includes = extract_includes(edges, files)
    write_csv(
        OUT_DIR / "files.csv",
        list(files.values()),
        headers=["id", "name"],
    )
    write_csv(
        OUT_DIR / "includes.csv",
        includes,
        headers=["source_file", "target_file"],
    )

    # 5) Summary
    summary = {
        "functions": len(func_records),
        "calls": len(calls),
        "types": len(types),
        "members": len(members),
    "files": len(files),
    "includes": len(includes),
        "export_json": str(EXPORT_JSON),
        "out_dir": str(OUT_DIR),
    }
    write_json(OUT_DIR / "summary.json", summary)
    print("[✔] Done. Outputs at:", OUT_DIR)
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
