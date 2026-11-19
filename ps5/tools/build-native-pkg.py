#!/usr/bin/env python3
"""Build a native PS5 PKG with the verified PS5 UltraPack 2.6.1 Linux writer.

Supply its released ffpfsc-pkg-tool executable with --tool. The application must
contain Encore's own metadata module, built by link-title.sh; no host-tool
binaries or imported entitlement modules are distributed with the game.
"""
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile

TOOL_SHA256 = 'd6f7ed59effcb23ce37ae96205c7b8ed502858b412828ad76c5cc45780211988'
TOOL_RELEASE = 'https://github.com/knutwurst/ps5-ultrapack/releases/tag/v2.6.1'


def digest(path):
    result = hashlib.sha256()
    with path.open('rb') as file:
        for block in iter(lambda: file.read(1024 * 1024), b''):
            result.update(block)
    return result.hexdigest()


def build(folder: Path, output: Path, tool: Path):
    folder, output, tool = folder.resolve(), output.resolve(), tool.resolve()
    if digest(tool) != TOOL_SHA256:
        raise ValueError(f'Use the Linux ffpfsc-pkg-tool from {TOOL_RELEASE}')
    subprocess.run(['python3', str(Path(__file__).with_name('check-no-pc-assets.py')), '--app', str(folder)], check=True)
    param = json.loads((folder / 'sce_sys/param.json').read_text())
    title, content, version = (param[k] for k in ('titleId', 'contentId', 'contentVersion'))
    if not (title.startswith('PPSA') and len(title) == 9 and title[4:].isdigit() and
            len(content) == 36 and content[7:16] == title):
        raise ValueError('Invalid package title or content identifier')
    about = folder / 'sce_sys/about/right.sprx'
    if not about.is_file() or not (folder / 'eboot.bin').is_file():
        raise ValueError('Build the application and its owned metadata module first')
    forbidden = {'.env', 'userdata', 'screenshots', 'klog', 'test-run.txt',
                 'test-results.txt', 'radv-shader-cache', 'songs', 'pc_bundled_songs'}
    for path in folder.rglob('*'):
        if path.is_symlink() or path.name in forbidden or path.relative_to(folder).parts[0] == 'songs':
            raise ValueError(f'Unexpected release input: {path.relative_to(folder)}')
    original = {str(p.relative_to(folder)): digest(p) for p in
                [folder / 'eboot.bin', folder / 'sce_sys/param.json', about]}
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='encore-pkg-', dir=output.parent) as directory:
        work = Path(directory)
        env = dict(os.environ, DOTNET_SYSTEM_GLOBALIZATION_INVARIANT='1',
                   DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_NOLOGO='1')
        subprocess.run([str(tool), 'build', str(folder), str(work),
                        '--content-id', content, '--title-id', title,
                        '--title', 'CH: Encore', '--version', version,
                        '--deterministic', '--no-fake-sign', '--no-retail-normalize',
                        '--no-ampr-index'], check=True, env=env)
        files = list(work.glob('*.pkg'))
        if len(files) != 1:
            raise ValueError('The writer did not produce exactly one package')
        result = subprocess.run([str(tool), 'validate', str(files[0]), '--json'],
                                check=True, capture_output=True, text=True, env=env)
        validation = json.loads(result.stdout)
        if validation['fail']:
            raise ValueError('Native package validation failed')
        for name, expected in original.items():
            if digest(folder / name) != expected:
                raise ValueError('Application changed during package creation')
        files[0].replace(output)
    return dict(builder=TOOL_RELEASE, tool_sha256=TOOL_SHA256,
                title=title, version=version, bytes=output.stat().st_size,
                sha256=digest(output), imported_modules=False,
                validation_failures=validation['fail'], validation_warnings=validation['warn'])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tool', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(build(args.folder, args.output, args.tool)))
