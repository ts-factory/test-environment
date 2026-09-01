# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Tests for the textual parameter-doc extraction and checks."""

from pathlib import Path

import cparam
from model import Param, Test, Value, param_doc_lines

DOCUMENTED = """\
/** @defgroup d-t T
 * @objective O
 */
int
main(void)
{
    int mode;
    unsigned int size;

    TEST_PARAM_DOC(mode,
        "Operation mode:",
        "- fast: skip checks, always",
        "- safe: full validation");
    TEST_GET_INT_PARAM(mode);
    TEST_PARAM_DOC(env, "Testing environment:", "- @ref env-peer2peer");
    TEST_GET_UINT_PARAM(size);
    return 0;
}
"""


def test_extract_docs() -> None:
    assert cparam.extract_docs(DOCUMENTED) == [
        (
            'mode',
            'Operation mode:\n- fast: skip checks, always\n- safe: full validation',
        ),
        ('env', 'Testing environment:\n- @ref env-peer2peer'),
    ]


def test_extract_docs_skips_directives() -> None:
    text = '#define MY_DOC(v_) \\\n    TEST_PARAM_DOC(v_, "x")\nint main(void) { return 0; }\n'
    assert cparam.extract_docs(text) == []


def test_extract_reads() -> None:
    assert cparam.extract_reads(DOCUMENTED) == ['mode', 'size']


def test_extract_reads_covers_wrappers_not_defines() -> None:
    text = (
        '#define TEST_GET_LED_PARAM(var_name_) \\\n'
        '    TEST_GET_ENUM_PARAM(var_name_, LEDS)\n'
        'int main(void)\n{\n    TEST_GET_LED_PARAM(led);\n    return 0;\n}\n'
    )
    assert cparam.extract_reads(text) == ['led']


def test_check_docs_findings() -> None:
    findings = cparam.check_docs(DOCUMENTED, 'd/t', strict=False)
    assert findings == ['d/t: parameter size is undocumented']


def test_check_docs_env_not_stale() -> None:
    findings = cparam.check_docs(DOCUMENTED, 'd/t', strict=True)
    assert not any('env' in f for f in findings)


def test_check_docs_header_counts_unless_strict() -> None:
    text = """\
/** @defgroup d-t T
 * @objective O
 *
 * @param size  The size
 */
int
main(void)
{
    unsigned int size;

    TEST_GET_UINT_PARAM(size);
    return 0;
}
"""
    assert cparam.check_docs(text, 'd/t', strict=False) == []
    assert cparam.check_docs(text, 'd/t', strict=True) == [
        'd/t: parameter size has no TEST_PARAM_DOC'
    ]


def test_check_docs_stale_and_duplicate() -> None:
    text = """\
int
main(void)
{
    TEST_PARAM_DOC(gone, "no such read");
    TEST_PARAM_DOC(mode, "one");
    TEST_PARAM_DOC(mode, "two");
    TEST_GET_INT_PARAM(mode);
    return 0;
}
"""
    findings = cparam.check_docs(text, 'd/t', strict=False)
    assert 'd/t: stale TEST_PARAM_DOC for gone (no such read)' in findings
    assert 'd/t: duplicate TEST_PARAM_DOC for mode' in findings


def test_param_doc_lines_matches_header_shape() -> None:
    p = Param(
        name='state',
        description='The state of the device',
        values=[Value(name='UP', comment='running'), Value(name='DOWN')],
    )
    assert param_doc_lines(p, ['state'], ['UP', 'DOWN']) == [
        'The state of the device:',
        '- @c UP (running)',
        '- @c DOWN',
    ]


def test_doc_drift() -> None:
    test = Test(
        name='t',
        summary='T',
        path=Path('d/package.md'),
        line=1,
        params=[Param(name='mode', description='Operation mode')],
    )
    clean = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode, "Operation mode");\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    assert cparam.doc_drift(test, clean, 'd/t') == []
    drifted = clean.replace('"Operation mode"', '"Something else"')
    findings = cparam.doc_drift(test, drifted, 'd/t')
    assert len(findings) == 1
    assert 'mode' in findings[0]
    assert 'differs' in findings[0]


def test_doc_drift_values_optional_description_only() -> None:
    """Values on the md side, but the inline doc has only the text.

    Documenting the value list is optional (spec): a description-only
    inline doc must not drift against a package.md param that has
    values.
    """
    test = Test(
        name='t',
        summary='T',
        path=Path('d/package.md'),
        line=1,
        params=[
            Param(
                name='mode',
                description='Operation mode',
                values=[Value(name='fast'), Value(name='safe')],
            )
        ],
    )
    text = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode, "Operation mode:");\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    assert cparam.doc_drift(test, text, 'd/t') == []


def test_doc_drift_values_free_style() -> None:
    """Free-style value lines (not the '- @c value' shape) do not drift."""
    test = Test(
        name='t',
        summary='T',
        path=Path('d/package.md'),
        line=1,
        params=[
            Param(
                name='mode',
                description='Operation mode',
                values=[Value(name='fast'), Value(name='safe')],
            )
        ],
    )
    text = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode,\n'
        '        "Operation mode:",\n'
        '        "- fast: skip checks");\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    assert cparam.doc_drift(test, text, 'd/t') == []


def test_doc_drift_description_change_still_flagged() -> None:
    """A changed description still drifts even when values are present."""
    test = Test(
        name='t',
        summary='T',
        path=Path('d/package.md'),
        line=1,
        params=[
            Param(
                name='mode',
                description='Operation mode',
                values=[Value(name='fast'), Value(name='safe')],
            )
        ],
    )
    text = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode,\n'
        '        "Something else:",\n'
        '        "- fast: skip checks");\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    findings = cparam.doc_drift(test, text, 'd/t')
    assert len(findings) == 1
    assert 'mode' in findings[0]
    assert 'differs' in findings[0]


def test_check_docs_empty_string() -> None:
    text = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode, "");\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    for strict in (False, True):
        findings = cparam.check_docs(text, 'd/t', strict=strict)
        assert findings == ['d/t: empty TEST_PARAM_DOC for mode']


def test_check_docs_whitespace_only() -> None:
    text = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode, "   ");\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    for strict in (False, True):
        findings = cparam.check_docs(text, 'd/t', strict=strict)
        assert findings == ['d/t: empty TEST_PARAM_DOC for mode']


def test_check_docs_non_string_arg_is_empty() -> None:
    """A non-literal arg (e.g. a macro name) extracts as empty text."""
    text = (
        'int main(void)\n{\n'
        '    TEST_PARAM_DOC(mode, FOO);\n'
        '    TEST_GET_INT_PARAM(mode);\n    return 0;\n}\n'
    )
    assert cparam.extract_docs(text) == [('mode', '')]
    for strict in (False, True):
        findings = cparam.check_docs(text, 'd/t', strict=strict)
        assert findings == ['d/t: empty TEST_PARAM_DOC for mode']


def test_check_docs_empty_suppresses_header_fallback() -> None:
    """An empty inline doc is a defect even when a header @param exists."""
    text = """\
/** @defgroup d-t T
 * @objective O
 *
 * @param mode  The mode
 */
int
main(void)
{
    TEST_PARAM_DOC(mode, "");
    TEST_GET_INT_PARAM(mode);
    return 0;
}
"""
    for strict in (False, True):
        findings = cparam.check_docs(text, 'd/t', strict=strict)
        assert findings == ['d/t: empty TEST_PARAM_DOC for mode']
