#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
轻量包装：从父级同名脚本导入主要逻辑；若导入失败，则提供最小可运行的占位实现，
确保输出写到上一级 graph_export_analysis 目录，方便与同目录其他脚本配合。
"""
from __future__ import annotations
from pathlib import Path
import importlib.util
import sys
import json

HERE = Path(__file__).resolve().parent
PARENT_SCRIPT = HERE.parent / "parse_graphson_export.py"
OUT_DIR = HERE.parent / "graph_export_analysis"


def _run_parent():
    spec = importlib.util.spec_from_file_location("parse_graphson_export_parent", str(PARENT_SCRIPT))
    if spec and spec.loader:
        mod = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = mod
        spec.loader.exec_module(mod)  # type: ignore[attr-defined]
        # 如果父脚本暴露 main(args) 或 run(output_dir=...) 接口，尽量调用
        if hasattr(mod, "main"):
            try:
                return mod.main()
            except TypeError:
                pass
        if hasattr(mod, "run"):
            try:
                return mod.run(output_dir=str(OUT_DIR))
            except TypeError:
                return mod.run()
        # 否则直接完成导入即可（父脚本可能顶层执行）
        return None
    raise FileNotFoundError(f"无法导入父级脚本: {PARENT_SCRIPT}")


def _fallback_minimal():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUT_DIR / "summary.json").write_text(json.dumps({
        "note": "fallback stub ran; parent parse_graphson_export.py not imported",
    }, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Wrote fallback summary at {OUT_DIR}")


if __name__ == "__main__":
    try:
        _run_parent()
    except Exception as e:
        print(f"[warn] parent import/run failed: {e}")
        _fallback_minimal()
