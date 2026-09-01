# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Textual harvest of enum parameter value mappings.

TAPI and suite headers define the allowed values of enum test
parameters as mapping-list macros ({ "NAME", VALUE } initializer
entries) and read them through wrapper getters expanding to
TEST_GET_ENUM_PARAM(var, SOME_MAPPING_LIST).  The documentation
build parses tests without those headers, so this module recovers
both artifacts with regular expressions from the header text:
value names for the doc pages, wrapper names so the degraded parse
can be told to record their uses.  Numeric values are deliberately
not resolved - documentation needs the names only.
"""

from __future__ import annotations

import re
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from collections.abc import Iterable, Iterator
    from pathlib import Path

_DEFINE = re.compile(r'\s*#\s*define\s+(\w+)')
_ENTRY = re.compile(r'\{\s*"([^"]+)"\s*,')
_WRAPPER = re.compile(r'\bTEST_GET_ENUM_PARAM\s*\(\s*\w+\s*,\s*(\w+)\s*\)')


def _defines(text: str) -> Iterator[tuple[str, str]]:
    """(name, full definition text) per #define, continuations joined."""
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        m = _DEFINE.match(lines[i])
        if m is None:
            i += 1
            continue
        body = [lines[i]]
        while lines[i].rstrip().endswith('\\') and i + 1 < len(lines):
            i += 1
            body.append(lines[i])
        yield m.group(1), '\n'.join(body)
        i += 1


def harvest_text(text: str) -> tuple[dict[str, str], dict[str, list[str]]]:
    """Wrapper getters and value mappings defined in one source text.

    Returns:
        (wrapper name -> mapping macro name) for every macro whose
        definition reads through TEST_GET_ENUM_PARAM with a single
        mapping macro, and (mapping macro name -> value names, in
        definition order) for every macro defined as { "name",
        value } entries.
    """
    wrappers: dict[str, str] = {}
    mappings: dict[str, list[str]] = {}
    for name, body in _defines(text):
        entries = _ENTRY.findall(body)
        if entries:
            mappings[name] = entries
            continue
        m = _WRAPPER.search(body)
        if m is not None:
            wrappers[name] = m.group(1)
    return wrappers, mappings


def harvest(paths: Iterable[Path]) -> tuple[dict[str, str], dict[str, list[str]]]:
    """Merged harvest over files; earlier paths win on name clashes.

    Unreadable paths are skipped: the caller lists candidate
    headers, not guaranteed ones.
    """
    wrappers: dict[str, str] = {}
    mappings: dict[str, list[str]] = {}
    for path in paths:
        try:
            text = path.read_text(encoding='utf-8', errors='replace')
        except OSError:
            continue
        file_wrappers, file_mappings = harvest_text(text)
        for name, mapping in file_wrappers.items():
            wrappers.setdefault(name, mapping)
        for name, values in file_mappings.items():
            mappings.setdefault(name, values)
    return wrappers, mappings
