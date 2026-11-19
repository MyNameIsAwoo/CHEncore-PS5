#!/usr/bin/env python3
"""Shape the native converter's PIC output as an initializer-only PS5 module.

The module returns its versioned engine entry through runtime_module_api.h;
no application exports or name-based symbol lookup are required.
"""
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
import argparse
import hashlib
import struct
import subprocess
from pathlib import Path


def convert(source: bytes) -> bytes:
    data = bytearray(source)
    if len(data) < 64 or data[:9] != b'\x7fELF\x02\x01\x01\x09\x02':
        raise ValueError('Expected a native PS5 ELF64 image')
    kind, machine = struct.unpack_from('<HH', data, 16)
    entry, phoff = struct.unpack_from('<QQ', data, 24)
    phsize, count = struct.unpack_from('<HH', data, 54)
    if kind != 0xfe10 or machine != 62 or phsize != 56 or count != 14 or phoff + count * phsize > len(data):
        raise ValueError('Unexpected native module layout')
    notes, parameters, dynamic, executable = [], [], [], False
    for i in range(count):
        at = phoff + i * phsize
        tag, flags, offset, address, _, files, memory, alignment = struct.unpack_from('<IIQQQQQQ', data, at)
        if offset + files > len(data):
            raise ValueError('Program segment exceeds the input')
        if tag == 1:
            if alignment > 1 and offset % alignment != address % alignment:
                raise ValueError('Unaligned load segment')
            executable |= bool(flags & 1 and address <= entry < address + files)
        elif tag == 0x61000001:
            parameters.append((at, offset, files))
        elif tag == 2:
            dynamic.append((offset, files))
        elif tag == 4:
            notes.append((offset, files))
    if not executable or len(parameters) != 1 or len(dynamic) != 1 or len(notes) != 2:
        raise ValueError('Missing native entry, parameters, dynamic table or build notes')
    at, offset, length = parameters[0]
    if length < 32:
        raise ValueError('Truncated native process parameters')
    companion_sdk, module_sdk = struct.unpack_from('<II', data, offset + 16)
    struct.pack_into('<H', data, 16, 0xfe18)
    struct.pack_into('<Q', data, 24, 0)
    struct.pack_into('<I', data, at, 0x61000002)
    struct.pack_into('<QQ', data, at + 32, 32, 32)
    data[offset:offset + 32] = struct.pack('<QIIIIII', 32, 0x3c13f4bf, 3, companion_sdk, module_sdk, 1, 0)
    start, size = dynamic[0]
    if size % 16:
        raise ValueError('Misaligned dynamic table')
    original, debug, initialized = None, None, False
    for at in range(start, start + size, 16):
        tag, value = struct.unpack_from('<QQ', data, at)
        if tag == 12:
            struct.pack_into('<Q', data, at + 8, entry)
            initialized = True
        elif tag in (13, 25, 26, 27, 28, 32, 33):
            # runtime_module_crt runs the constructor arrays once itself.
            struct.pack_into('<Q', data, at + 8, 0)
        elif tag == 0x61000041:
            original = value
        elif tag == 21:
            debug = at
    if original is None or debug is None or not initialized:
        raise ValueError('Missing native filename or initialization metadata')
    struct.pack_into('<QQ', data, debug, 14, original)  # DT_SONAME
    for (offset, size), length in zip(notes, (16, 8)):
        if size < 16 + length:
            raise ValueError('Truncated build note')
        data[offset + 16:offset + 16 + length] = bytes(length)
    digest = hashlib.sha1(data).digest()
    for (offset, _), length in zip(notes, (16, 8)):
        data[offset + 16:offset + 16 + length] = digest[:length]
    return bytes(data)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tool', required=True, type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    intermediate = args.input.with_suffix('.module.elf')
    intermediate.write_bytes(convert(args.input.read_bytes()))
    subprocess.run([str(args.tool), 'self', '--sign', '--in', str(intermediate), '--out', str(args.output)], check=True)
