# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Tests for the textual value-mapping harvest."""

from cmaps import harvest, harvest_text

HEADER = """\
/** The list of values allowed for 'test_ethdev_state' */
#define ETHDEV_STATE_MAPPING_LIST \\
            { "INITIALIZED", (int)TEST_ETHDEV_INITIALIZED },  \\
            { "CONFIGURED", (int)TEST_ETHDEV_CONFIGURED },    \\
            { "STARTED", (int)TEST_ETHDEV_STARTED }

#define TEST_GET_ETHDEV_STATE(var_name_) \\
    TEST_GET_ENUM_PARAM(var_name_, ETHDEV_STATE_MAPPING_LIST)

#define TEST_GET_BOOL_PARAM(var_name_) \\
    TEST_GET_ENUM_PARAM(var_name_, BOOL_MAPPING_LIST)

#define UNRELATED(x) do { } while (0)
#define WIDTH 42
"""


def test_harvest_mappings_and_wrappers() -> None:
    wrappers, mappings = harvest_text(HEADER)
    assert mappings['ETHDEV_STATE_MAPPING_LIST'] == [
        'INITIALIZED',
        'CONFIGURED',
        'STARTED',
    ]
    assert wrappers['TEST_GET_ETHDEV_STATE'] == 'ETHDEV_STATE_MAPPING_LIST'
    assert wrappers['TEST_GET_BOOL_PARAM'] == 'BOOL_MAPPING_LIST'
    assert 'UNRELATED' not in wrappers
    assert 'WIDTH' not in wrappers
    assert 'WIDTH' not in mappings


def test_harvest_single_line_definitions() -> None:
    text = (
        '#define MODE_MAP { "A", 1 }, { "B", 2 }\n'
        '#define GET_MODE(v_) TEST_GET_ENUM_PARAM(v_, MODE_MAP)\n'
    )
    wrappers, mappings = harvest_text(text)
    assert mappings == {'MODE_MAP': ['A', 'B']}
    assert wrappers == {'GET_MODE': 'MODE_MAP'}


def test_harvest_files_earlier_paths_win(tmp_path) -> None:
    first = tmp_path / 'a.h'
    second = tmp_path / 'b.h'
    first.write_text('#define M { "LOCAL", 1 }\n', encoding='utf-8')
    second.write_text(
        '#define M { "SHARED", 1 }\n#define GET_X(v_) TEST_GET_ENUM_PARAM(v_, M)\n',
        encoding='utf-8',
    )
    wrappers, mappings = harvest([first, second, tmp_path / 'missing.h'])
    assert mappings['M'] == ['LOCAL']
    assert wrappers == {'GET_X': 'M'}
