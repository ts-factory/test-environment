# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Extract parameter reads and inline docs from C source text.

The textual counterpart of the token-level extraction in aststeps:
scenario check runs without libclang, so it recovers TEST_PARAM_DOC
uses and TEST_GET_*_PARAM reads with regular expressions - the
same trade-off cstep makes for steps (a use inside a comment does
match).  Any TEST_GET_*_PARAM spelling counts as a read of its
first argument, which covers suite-local wrapper macros at their
use site; the env parameter is invisible here (its read hides
inside TEST_START), so env docs are never reported stale.
"""

from __future__ import annotations

import re
from typing import TYPE_CHECKING

from cheader import parse_doc_header
from cstep import DOC_ESCAPES, unescape
from model import normalize_ws, resolve_inline

if TYPE_CHECKING:
    from model import Test

_DOC_CALL = re.compile(r'\bTEST_PARAM_DOC\s*\(\s*(\w+)\s*,')
_READ_CALL = re.compile(r'\bTEST_GET_\w+_PARAM\s*\(\s*(\w+)')
_STRING = re.compile(r'\s*"((?:[^"\\]|\\.)*)"')


def _strip_directives(text: str) -> str:
    """Source text with preprocessor directive lines blanked.

    A directive spans backslash-continued lines, so a getter use
    inside a #define does not count as a read of the macro's own
    argument name.
    """
    out = []
    cont = False
    for line in text.splitlines():
        if cont or line.lstrip().startswith('#'):
            cont = line.rstrip().endswith('\\')
            out.append('')
        else:
            out.append(line)
    return '\n'.join(out)


def extract_docs(text: str) -> list[tuple[str, str]]:
    """(name, text) per TEST_PARAM_DOC use, in source order.

    Each top-level argument after the name is one line; adjacent
    string literals within an argument concatenate.  Duplicates
    are preserved for the checker to flag.
    """
    body = _strip_directives(text)
    docs: list[tuple[str, str]] = []
    for m in _DOC_CALL.finditer(body):
        pos = m.end()
        depth = 1
        lines: list[str] = []
        cur: list[str] = []
        while pos < len(body) and depth > 0:
            sm = _STRING.match(body, pos)
            if sm is not None:
                cur.append(unescape(sm.group(1), DOC_ESCAPES))
                pos = sm.end()
                continue
            ch = body[pos]
            if ch == '(':
                depth += 1
            elif ch == ')':
                depth -= 1
            elif ch == ',' and depth == 1:
                lines.append(''.join(cur))
                cur = []
            pos += 1
        lines.append(''.join(cur))
        docs.append((m.group(1), '\n'.join(lines)))
    return docs


def extract_reads(text: str) -> list[str]:
    """Parameter names read via any TEST_GET_*_PARAM, source order."""
    return [m.group(1) for m in _READ_CALL.finditer(_strip_directives(text))]


def check_docs(text: str, c_name: str, *, strict: bool) -> list[str]:
    """Parameter documentation findings for one implemented test.

    Undocumented: a read with no TEST_PARAM_DOC (the transitional
    default also accepts a header @param; strict does not, with a
    finding text that says what is actually missing).  A doc whose
    text is empty or whitespace-only does not count as documentation
    (its own finding covers that; it is not also reported as
    undocumented).  Stale: a doc without a read, except env.
    Duplicate: two docs for one name.  An empty doc still occupies
    its name for duplicate/stale purposes.
    """
    docs = extract_docs(text)
    doc_names = [name for name, _ in docs]
    empty_names = {name for name, doc_text in docs if not doc_text.strip()}
    reads = list(dict.fromkeys(extract_reads(text)))
    header = {name for name, _ in parse_doc_header(text)[3]}
    findings: list[str] = []
    for name in reads:
        if name in doc_names:
            continue
        if strict and name in header:
            findings.append(f'{c_name}: parameter {name} has no TEST_PARAM_DOC')
        elif name not in header:
            findings.append(f'{c_name}: parameter {name} is undocumented')
    read_set = set(reads)
    for name in dict.fromkeys(doc_names):
        if name in empty_names:
            findings.append(f'{c_name}: empty TEST_PARAM_DOC for {name}')
        if doc_names.count(name) > 1:
            findings.append(f'{c_name}: duplicate TEST_PARAM_DOC for {name}')
        if name not in read_set and name != 'env':
            findings.append(f'{c_name}: stale TEST_PARAM_DOC for {name} (no such read)')
    return findings


def doc_drift(test: Test, text: str, c_name: str) -> list[str]:
    """Description drift between package.md params and inline docs.

    Only parameters present on both sides are compared; coverage
    is check_docs' business.  Only the description part is checked:
    the value list is the author's free choice per the spec, so the
    inline doc's description (its leading text, before any bullets
    or extra lines) must match package.md, but need not repeat or
    even mention the values at all.
    """
    names = [p.name for p in test.params]
    values = [v.name for p in test.params for v in p.values]
    docs = dict(extract_docs(text))
    findings: list[str] = []
    for p in test.params:
        c_doc = docs.get(p.name)
        if c_doc is None:
            continue
        md_desc = normalize_ws(resolve_inline(p.description, names, values)).rstrip('.:')
        c_text = normalize_ws(c_doc)
        if not c_text.startswith(md_desc):
            findings.append(
                f'{c_name}: parameter {p.name} description differs:\n'
                f'  md: {md_desc}\n  c:  {c_text}'
            )
    return findings
