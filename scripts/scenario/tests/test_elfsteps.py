# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Tests for the te_scenario ELF section reader."""

import shutil
import subprocess
import textwrap
from pathlib import Path

import pytest

from elfsteps import read_scenario

CC = shutil.which('cc')

FIXTURE = """\
#define CAT_(a_, b_) a_##b_
#define CAT(a_, b_) CAT_(a_, b_)
#define RECORD(kind, fmt) \\
    static const char CAT(rec_, __COUNTER__)[] \\
        __attribute__((used, section("te_scenario"))) = \\
        kind "\\0" __FILE__ "\\0" "1" "\\0" fmt

RECORD("STEP", "Initialize EAL and configure @p iut_port.");
RECORD("SUBSTEP", "Send a packet of size @p mtu.");
RECORD("POP", "");

int main(void) { return 0; }
"""


@pytest.mark.skipif(CC is None, reason='no C compiler')
def test_read_scenario(tmp_path: Path) -> None:
    src = tmp_path / 'fix.c'
    src.write_text(textwrap.dedent(FIXTURE), encoding='utf-8')
    binary = tmp_path / 'fix'
    subprocess.run(  # noqa: S603 - fixed argv, no input
        [str(CC), '-o', str(binary), str(src)], check=True
    )
    recs = read_scenario(binary)
    assert [(r.kind, r.text) for r in recs] == [
        ('STEP', 'Initialize EAL and configure @p iut_port.'),
        ('SUBSTEP', 'Send a packet of size @p mtu.'),
        ('POP', ''),
    ]
    assert recs[0].file.endswith('fix.c')
    assert recs[0].line == 1


def test_no_section(tmp_path: Path) -> None:
    f = tmp_path / 'not-elf'
    f.write_bytes(b'hello world')
    with pytest.raises(ValueError, match='ELF'):
        read_scenario(f)
