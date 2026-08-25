# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Extract TEST_STEP sequences from C and compare with markdown."""

from __future__ import annotations

import re
from typing import TYPE_CHECKING

from model import flatten_steps, normalize_ws, resolve_inline

if TYPE_CHECKING:
    from model import Test

_CALL = re.compile(r'\bTEST_(STEP|SUBSTEP|STEP_PUSH|STEP_NEXT|STEP_POP)\s*\(')
_STRING = re.compile(r'\s*"((?:[^"\\]|\\.)*)"')


def _unescape(s: str) -> str:
    return s.replace('\\"', '"').replace('\\n', ' ').replace('\\t', ' ')


def extract_steps(text: str) -> list[tuple[int, str]]:
    """Recover the (depth, text) step sequence from C source text."""
    steps: list[tuple[int, str]] = []
    prev = 0
    for m in _CALL.finditer(text):
        kind = m.group(1)
        pos = m.end()
        parts: list[str] = []
        while True:
            sm = _STRING.match(text, pos)
            if sm is None:
                break
            parts.append(_unescape(sm.group(1)))
            pos = sm.end()
        if kind == 'STEP':
            depth = 1
        elif kind == 'SUBSTEP':
            depth = 2
        elif kind == 'STEP_PUSH':
            depth = prev + 1
        elif kind == 'STEP_NEXT':
            depth = prev
        else:  # STEP_POP
            depth = prev - 1
        prev = depth
        step_text = normalize_ws(''.join(parts))
        if step_text:
            steps.append((depth, step_text))
    return steps


def compare(test: Test, c_text: str, c_name: str) -> list[str]:
    """Compare a markdown scenario against implemented C source."""
    names = [p.name for p in test.params]
    values = [v.name for p in test.params for v in p.values]
    md = [
        (d, normalize_ws(resolve_inline(s.text, names, values)))
        for d, s in flatten_steps(test.steps)
    ]
    c = extract_steps(c_text)
    findings: list[str] = []
    for n, ((md_d, md_t), (c_d, c_t)) in enumerate(zip(md, c), 1):
        if md_t != c_t:
            findings.append(f'{c_name}: step {n} text differs:\n  md: {md_t}\n  c:  {c_t}')
        elif md_d != c_d:
            findings.append(f'{c_name}: step {n} depth differs: md {md_d}, c {c_d}')
    if len(md) != len(c) and not findings:
        findings.append(f'{c_name}: {len(md)} steps in markdown, {len(c)} in C')
    return findings
