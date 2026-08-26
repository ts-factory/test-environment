# SPDX-License-Identifier: Apache-2.0
# Copyright (C) 2026 OKTET Ltd.
"""Reader of the te_scenario ELF section of a built test."""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from pathlib import Path

_MAGIC = b'\x7fELF'


@dataclass
class StepRecord:
    """One recorded step."""

    kind: str
    file: str
    line: int
    text: str


def _section_data(blob: bytes, path: Path) -> bytes:
    if blob[:4] != _MAGIC:
        msg = f'{path}: not an ELF file'
        raise ValueError(msg)
    ei_class, ei_data = blob[4], blob[5]
    if ei_class != 2:  # noqa: PLR2004 - ELFCLASS64
        msg = f'{path}: only 64-bit ELF is supported'
        raise ValueError(msg)
    end = '<' if ei_data == 1 else '>'
    e_shoff = struct.unpack_from(f'{end}Q', blob, 0x28)[0]
    e_shentsize = struct.unpack_from(f'{end}H', blob, 0x3A)[0]
    e_shnum = struct.unpack_from(f'{end}H', blob, 0x3C)[0]
    e_shstrndx = struct.unpack_from(f'{end}H', blob, 0x3E)[0]

    def shdr(i: int) -> tuple[int, int, int]:
        base = e_shoff + i * e_shentsize
        name = struct.unpack_from(f'{end}I', blob, base)[0]
        offset = struct.unpack_from(f'{end}Q', blob, base + 0x18)[0]
        size = struct.unpack_from(f'{end}Q', blob, base + 0x20)[0]
        return name, offset, size

    _, str_off, _ = shdr(e_shstrndx)
    for i in range(e_shnum):
        name_off, offset, size = shdr(i)
        name_end = blob.index(b'\0', str_off + name_off)
        if blob[str_off + name_off : name_end] == b'te_scenario':
            return blob[offset : offset + size]
    msg = f'{path}: no te_scenario section'
    raise ValueError(msg)


def _read_field(data: bytes, pos: int, path: Path) -> tuple[bytes, int]:
    end = data.find(b'\0', pos)
    if end == -1:
        msg = f'{path}: malformed te_scenario section'
        raise ValueError(msg)
    return data[pos:end], end + 1


def read_scenario(path: Path) -> list[StepRecord]:
    """Read the recorded step list from a built test binary.

    Records are not guaranteed to pack back to back: a compiler may
    insert NUL alignment padding between the per-record arrays, so
    this walks the buffer instead of doing a naive split. Padding
    can only fall between records, before a kind field (which is
    never itself empty); the file/line/text fields that follow are
    read as-is, empty or not, so an empty text field (e.g. a "POP"
    record) is not mistaken for padding.
    """
    data = _section_data(path.read_bytes(), path)
    pos, size = 0, len(data)
    records = []
    while pos < size:
        while pos < size and data[pos] == 0:
            pos += 1
        if pos >= size:
            break
        kind_b, pos = _read_field(data, pos, path)
        file_b, pos = _read_field(data, pos, path)
        line_b, pos = _read_field(data, pos, path)
        text_b, pos = _read_field(data, pos, path)
        kind, file, line, text = (
            f.decode('utf-8', errors='replace') for f in (kind_b, file_b, line_b, text_b)
        )
        records.append(StepRecord(kind=kind, file=file, line=int(line), text=text))
    return records
