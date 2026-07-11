"""Smoke tests for scripts/qml-lint-conventions.py.

These tests verify the linter can be imported and run from the repo root
without crashing. They guard against the previous `NameError: Violation`
when the helper classes were defined below their first use.
"""

from __future__ import annotations

import importlib.util
import subprocess
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
SCRIPT = REPO_ROOT / "scripts" / "qml-lint-conventions.py"


def test_module_imports_without_nameerror() -> None:
    spec = importlib.util.spec_from_file_location("qml_lint", SCRIPT)
    assert spec and spec.loader, SCRIPT
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[union-attr]
    assert hasattr(module, "Violation")
    assert hasattr(module, "ScopeTracker")


def test_violation_string_uses_colour() -> None:
    spec = importlib.util.spec_from_file_location("qml_lint_str", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)  # type: ignore[union-attr]
    v = module.Violation("foo.qml", 1, "section-order", "msg")
    text = str(v)
    assert "section-order" in text
    assert "foo.qml:1" in text
    assert "msg" in text


def test_script_runs_to_completion() -> None:
    completed = subprocess.run(
        [sys.executable, str(SCRIPT)],
        cwd=REPO_ROOT,
        text=True,
        capture_output=True,
        check=False,
    )
    # The linter exits 0 on clean trees and 1 when violations exist; what
    # we care about is that it no longer crashes with NameError.
    assert "NameError" not in completed.stderr
    assert "Traceback" not in completed.stderr
