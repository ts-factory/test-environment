# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Tests for the AST-based doxygen input filter."""

import textwrap
from pathlib import Path

import pytest

import aststeps
from aststeps import Cond
from doxfilter import filter_text, merge_params, render_dox
from steptree import Node

HEADER_SRC = """\
/* SPDX-License-Identifier: Apache-2.0 */

/** @defgroup demo-check Demo check
 * @ingroup demo
 * @{
 *
 * @objective Check something
 *
 * @param mode  The mode
 *
 * @author A U Thor <author@example.com>
 *
 * @par Scenario:
 */

/** A member doc that must not survive filtering */
int
main(void)
{
    return 0;
}

/** @} */
"""


def test_render_dox() -> None:
    tree = [
        Node(kind='STEP', text='Prepare'),
        Node(
            kind='COND',
            cond=Cond(kind='if', cond='mode > 0', desc='if (mode > 0)'),
            children=[
                Node(kind='STEP', text='Extra check'),
                Node(
                    kind='COND',
                    cond=Cond(kind='goto', cond='0', desc='if (0), reached by goto'),
                    children=[Node(kind='SUBSTEP', text='Recover')],
                ),
            ],
        ),
    ]
    assert render_dox(tree) == [
        '   -# Prepare',
        '   -# If `mode > 0`:',
        '      - Extra check',
        '      - Only on the error path:',
        '         - Recover',
    ]


FILTER_SRC = """\
/* SPDX-License-Identifier: Apache-2.0 */

/** @defgroup demo-check Demo check
 * @ingroup demo
 * @{
 *
 * @objective Check something
 *
 * @param mode  The mode
 *
 * @author A U Thor <author@example.com>
 *
 * @par Scenario:
 */

int
main(void)
{
    int mode;

    TEST_STEP("Prepare");
    if (mode > 0)
    {
        TEST_STEP("Extra check");
    }
    TEST_STEP("Run the loop:");
    for (mode = 0; mode < 3; mode++)
    {
        TEST_SUBSTEP("Iterate once");
    }
    return 0;
}

/** @} */
"""


@pytest.mark.skipif(not aststeps.HAVE_CLANG, reason='libclang not installed')
def test_filter_text(tmp_path: Path) -> None:
    src = tmp_path / 'demo.c'
    src.write_text(textwrap.dedent(FILTER_SRC), encoding='utf-8')
    out = filter_text(src)
    lines = out.splitlines()

    assert '/** @defgroup demo-check Demo check' in lines
    # The author block moves to the bottom of the comment, after
    # the scenario.
    author = lines.index(' * @author A U Thor <author@example.com>')
    assert lines.index('         - Iterate once') < author < lines.index(' */')
    assert lines.count(' * @par Scenario:') == 1
    assert '   -# Prepare' in lines
    assert '   -# If `mode > 0`:' in lines
    assert '      - Extra check' in lines
    assert '   -# Run the loop:' in lines
    assert '      - For each iteration (`mode = 0; mode < 3; mode++`):' in lines
    assert '         - Iterate once' in lines
    assert lines[-2] == ' */'
    assert lines[-1] == '/** @} */'
    # The scenario sits inside the header comment.
    assert lines.index('   -# Prepare') < lines.index(' */')


@pytest.mark.skipif(not aststeps.HAVE_CLANG, reason='libclang not installed')
def test_filter_text_adds_scenario_marker(tmp_path: Path) -> None:
    src = tmp_path / 'demo.c'
    bare = FILTER_SRC.replace(' * @par Scenario:\n', '')
    src.write_text(textwrap.dedent(bare), encoding='utf-8')
    out = filter_text(src)
    assert ' * @par Scenario:' in out.splitlines()


@pytest.mark.skipif(not aststeps.HAVE_CLANG, reason='libclang not installed')
def test_filter_text_no_steps(tmp_path: Path) -> None:
    src = tmp_path / 'demo.c'
    src.write_text(textwrap.dedent(HEADER_SRC), encoding='utf-8')
    out = filter_text(src)
    lines = out.splitlines()
    assert '/** @defgroup demo-check Demo check' in lines
    assert not any(line.lstrip().startswith(('-#', '- ')) for line in lines)
    assert lines[-1] == '/** @} */'


PARAM_MERGE_SRC = """\
/* SPDX-License-Identifier: Apache-2.0 */

/** @defgroup demo-doc Demo doc
 * @ingroup demo
 * @{
 *
 * @objective Check params
 *
 * @param env   Testing environment:
 *              - @ref env-peer2peer
 * @param mode  Stale header text
 *
 * @par Scenario:
 */

int
main(void)
{
    int mode;

    TEST_PARAM_DOC(mode,
        "Operation mode:",
        "- fast: skip checks",
        "- safe: full validation");
    TEST_GET_INT_PARAM(mode);
    TEST_STEP("Run");
    return 0;
}
"""


@pytest.mark.skipif(not aststeps.HAVE_CLANG, reason='libclang not installed')
def test_filter_merges_param_docs(tmp_path: Path) -> None:
    src = tmp_path / 'demo.c'
    src.write_text(textwrap.dedent(PARAM_MERGE_SRC), encoding='utf-8')
    lines = filter_text(src).splitlines()
    assert ' * @param env   Testing environment:' in lines
    assert ' * @param mode Operation mode:' in lines
    cont = ' * ' + ' ' * len('@param mode ')
    assert f'{cont}- fast: skip checks' in lines
    assert f'{cont}- safe: full validation' in lines
    assert not any('Stale header text' in line for line in lines)
    env_at = lines.index(' * @param env   Testing environment:')
    mode_at = lines.index(' * @param mode Operation mode:')
    assert env_at < mode_at < lines.index(' * @par Scenario:')


@pytest.mark.skipif(not aststeps.HAVE_CLANG, reason='libclang not installed')
def test_filter_warns_undocumented(
    tmp_path: Path, capsys: pytest.CaptureFixture[str]
) -> None:
    src = tmp_path / 'demo.c'
    plain = """\
/** @defgroup demo-w Demo warn
 * @objective Check
 *
 * @par Scenario:
 */
int
main(void)
{
    int size;

    TEST_GET_INT_PARAM(size);
    TEST_STEP("Run");
    return 0;
}
"""
    src.write_text(textwrap.dedent(plain), encoding='utf-8')
    filter_text(src)
    assert 'parameter size is undocumented' in capsys.readouterr().err


def test_merge_params_keeps_content_between_spans() -> None:
    header = [
        '/** @defgroup x-y Title',
        ' * @objective Check',
        ' *',
        ' * @param mode  Old mode text',
        ' *',
        ' * @note Keep this caveat',
        ' *       across merges',
        ' *',
        ' * @param size  The size',
        ' *',
        ' * @par Scenario:',
    ]
    docs = [aststeps.ParamDoc(name='mode', text='New mode text', line=1)]
    out = merge_params(header, docs)
    # The @note block sits between the two @param blocks in the
    # source; it must not be dropped even though both @param spans
    # around it collapse into one merged block elsewhere.
    assert ' * @note Keep this caveat' in out
    assert ' *       across merges' in out
    # The overridden @param is gone, its stale text with it; the
    # untouched one survives verbatim.
    assert not any('Old mode text' in line for line in out)
    assert ' * @param size  The size' in out
    assert ' * @param mode New mode text' in out


def test_merge_params_no_header_params_before_scenario() -> None:
    header = [
        '/** @defgroup x-y Title',
        ' * @objective Check',
        ' *',
        ' * @par Scenario:',
    ]
    docs = [aststeps.ParamDoc(name='mode', text='Info', line=1)]
    out = merge_params(header, docs)
    assert out == [
        '/** @defgroup x-y Title',
        ' * @objective Check',
        ' *',
        ' * @param mode Info',
        ' *',
        ' * @par Scenario:',
    ]


def test_merge_params_no_header_params_no_marker() -> None:
    header = [
        '/** @defgroup x-y Title',
        ' * @objective Check',
    ]
    docs = [aststeps.ParamDoc(name='mode', text='Info', line=1)]
    out = merge_params(header, docs)
    assert out == [
        '/** @defgroup x-y Title',
        ' * @objective Check',
        ' *',
        ' * @param mode Info',
    ]
