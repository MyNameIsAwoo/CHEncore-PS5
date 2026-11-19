# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Subprocess adapter for the separately downloaded GPL exFAT tool."""
import hashlib
from pathlib import Path
import sys

def main(arguments=None):
    dependencies, source, output = map(Path, sys.argv[1:] if arguments is None else arguments)
    sys.path.insert(0, str(dependencies))
    from mkpfs.exfat_writer import write_exfat_image
    from mkpfs.exfat import ExfatReader
    write_exfat_image(source, output)
    expected = {p.relative_to(source).as_posix(): p for p in source.rglob('*') if p.is_file()}
    checked = set()
    with output.open('rb') as f:
        reader = ExfatReader(f)
        for entry in reader.iter_files():
            if entry.rel_path not in expected or entry.rel_path in checked:
                raise ValueError('Unexpected or duplicate exFAT entry.')
            h = hashlib.sha256()
            for data in reader.read_file(entry): h.update(data)
            wanted = hashlib.sha256()
            with expected[entry.rel_path].open('rb') as original:
                for data in iter(lambda: original.read(1024 * 1024), b''): wanted.update(data)
            if h.hexdigest() != wanted.hexdigest():
                raise ValueError('exFAT file checksum failed: ' + entry.rel_path)
            checked.add(entry.rel_path)
    if checked != set(expected):
        raise ValueError('exFAT image is missing files.')
    print('Verified every file in the exFAT image.', flush=True)

if __name__ == '__main__': main()
