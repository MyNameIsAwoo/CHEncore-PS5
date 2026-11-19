# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Pinned packaging tools, fetched from their official public releases only.

Tools are separate processes. Nothing from them is injected into the title.
Their complete source/licence archives are retained in the user's tool cache.
"""
from __future__ import annotations
import json
import os
from pathlib import Path
import platform
import shutil
import tempfile
import urllib.request
import zipfile
from build_core import checkpoint, run_process, safe_name, sha_file, HERE

SOURCE_REV = '43c07b8dc24e96902e12eeaceee883a7bc68e390'
SOURCE_SHA = '6e85d2a055701d2ab9b39a92d339129ffa74eab77b9e4d1ef309b6e4e4680f46'
SOURCE_URL = 'https://codeload.github.com/knutwurst/ps5-ultrapack/zip/' + SOURCE_REV
PKG_TOOLS = {
    'Windows': ('PS5-UltraPack-2.6.1-windows-x64.zip',
                'c59c7c84c0280805548d8ebc65f5aa5a7c7742d6a8a916d45392c30483b14196',
                'ffpfsc-pkg-tool.exe', '3782a53ed3fb09ca32c9c1612c75b500f5002eaf62f1aaa7bef3a806af3c75f2'),
    'Linux': ('PS5-UltraPack-2.6.1-linux-x64.tar.gz',
              '3a8098c47514083f98411a547eaff65a886908cd351395066a8a081e421a100b',
              'ffpfsc-pkg-tool', 'd6f7ed59effcb23ce37ae96205c7b8ed502858b412828ad76c5cc45780211988'),
}

def cache_directory():
    if os.name == 'nt':
        return Path(os.environ.get('LOCALAPPDATA', Path.home())) / 'CHEncoreBuilder' / 'tools'
    return Path(os.environ.get('XDG_CACHE_HOME', Path.home() / '.cache')) / 'ch-encore-builder' / 'tools'

def download(url, target, wanted, cancel, log):
    if target.is_file() and sha_file(target) == wanted:
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    log('Downloading a verified packaging tool: ' + target.name)
    request = urllib.request.Request(url, headers={'User-Agent': 'CH-Encore-Install-Builder'})
    with tempfile.NamedTemporaryFile(prefix='.download-', dir=target.parent, delete=False) as f:
        temporary = Path(f.name)
        try:
            with urllib.request.urlopen(request, timeout=30) as source:
                while chunk := source.read(1024 * 1024):
                    checkpoint(cancel); f.write(chunk)
            f.close()
            if sha_file(temporary) != wanted:
                raise ValueError('Packaging tool checksum failed. No downloaded code was run.')
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    return target

def prepare_backend(kind, cache, pkg_tool, cancel, log):
    if kind == 'folder':
        return None
    embedded = HERE / 'embedded-tools'
    allow_pkg_download = False
    if (embedded / 'manifest.json').is_file() and not pkg_tool:
        manifest = json.loads((embedded / 'manifest.json').read_text(encoding='utf-8'))
        if manifest.get('schema') != 1 or manifest.get('engine') != 'packwright':
            raise ValueError('The embedded packaging tools are damaged.')
        executable = manifest.get('executable', '')
        safe_name(executable)
        if executable not in manifest.get('files', {}):
            raise ValueError('Missing embedded packaging executable.')
        for name, wanted in manifest['files'].items():
            checkpoint(cancel); safe_name(name)
            path = embedded / name
            if not path.is_file() or path.is_symlink() or sha_file(path) != wanted:
                raise ValueError('An embedded packaging file is missing or damaged: ' + name)
        # The embedded engine is qualified for exFAT. Its native PKG writer
        # omits installation metadata, so PKG uses the verified native writer
        # automatically cached from its official release instead.
        allow_pkg_download = kind == 'pkg' and manifest.get('pkg_policy') == 'verified-download'
        if not allow_pkg_download:
            if kind == 'pkg': raise ValueError('The executable needs its verified PKG backend configuration.')
            return {'packwright': embedded / executable}
    import sys
    if getattr(sys, 'frozen', False) and not pkg_tool and not allow_pkg_download:
        raise ValueError('The self-contained builder is missing its embedded tools. Download it again.')
    cache = Path(cache) if cache is not None else cache_directory()
    if kind == 'exfat':
        return download(SOURCE_URL, cache / 'ultrapack-2.6.1-source.zip', SOURCE_SHA, cancel, log)
    system = platform.system()
    if system not in PKG_TOOLS or platform.machine().lower() not in ('x86_64', 'amd64'):
        raise ValueError('Native PKG building currently supports Windows/Linux x64. Choose Folder or exFAT on this computer.')
    name, archive_hash, binary_name, binary_hash = PKG_TOOLS[system]
    if pkg_tool:
        tool = Path(pkg_tool).resolve()
        if sha_file(tool) != binary_hash:
            raise ValueError('Use ffpfsc-pkg-tool from PS5 UltraPack 2.6.1 for this operating system.')
        return tool
    archive = download('https://github.com/knutwurst/ps5-ultrapack/releases/download/v2.6.1/' + name,
                       cache / name, archive_hash, cancel, log)
    # Keep the official release archive (including notices), but extract only its
    # standalone writer and ImageMagick dependency, never any entitlement module.
    folder = cache / ('ultrapack-2.6.1-' + system.lower()); folder.mkdir(exist_ok=True)
    wanted = {binary_name, 'Magick.Native-Q8-x64.dll'} if system == 'Windows' else {binary_name}
    if system == 'Windows':
        with zipfile.ZipFile(archive) as z:
            for info in z.infolist():
                if '/_internal/backend/native/' in info.filename and Path(info.filename).name in wanted:
                    target = folder / Path(info.filename).name
                    with z.open(info) as source, target.open('wb') as output: shutil.copyfileobj(source, output)
    else:
        import tarfile
        with tarfile.open(archive) as t:
            for member in t:
                if member.isfile() and '/_internal/backend/native/' in member.name and Path(member.name).name in wanted:
                    with t.extractfile(member) as source, (folder / Path(member.name).name).open('wb') as output:
                        shutil.copyfileobj(source, output)
    tool = folder / binary_name
    if not tool.is_file() or sha_file(tool) != binary_hash:
        raise ValueError('The downloaded package writer did not match its expected checksum.')
    tool.chmod(0o755)
    return tool

def package(kind, app, output, backend, cancel, log):
    if isinstance(backend, dict):
        if kind != 'exfat':
            raise ValueError('The embedded engine is only qualified for exFAT; use the verified native PKG writer.')
        tool = backend['packwright']
        env = dict(os.environ, PACKWRIGHT_DATA=str(app.parent / 'packaging-state'),
                   DOTNET_SYSTEM_GLOBALIZATION_INVARIANT='1', DOTNET_CLI_TELEMETRY_OPTOUT='1')
        command = [tool, 'convert', app, '--to', kind, '-o', output, '--no-ampr']
        if kind == 'pkg': command += ['--compression', 'stored', '--deterministic', '--drm', 'free']
        run_process(command, cancel, log, env)
        if not output.is_file(): raise ValueError('The embedded tool did not create the selected output.')
        run_process([tool, 'verify', output], cancel, log, env)
        with tempfile.TemporaryDirectory(prefix='encore-check-', dir=output.parent) as directory:
            extracted = Path(directory) / 'title'
            run_process([tool, 'extract', output, '-o', extracted], cancel, log, env)
            pack_name = 'assets/EncoreAssets-v1.encore-assets'
            # Verify the payload that matters for personal setup and keep the
            # native executable/modules unchanged by this packaging backend.
            for name in [pack_name, 'eboot.bin', 'sce_module/encore-engine.prx', 'sce_sys/about/right.sprx']:
                if sha_file(app / name) != sha_file(extracted / name):
                    raise ValueError('Container readback changed a required title file: ' + name)
            original = json.loads((app / 'sce_sys/param.json').read_text())
            actual = json.loads((extracted / 'sce_sys/param.json').read_text())
            if any(original[key] != actual[key] for key in ('titleId', 'contentId', 'contentVersion')):
                raise ValueError('Container readback changed the title identity.')
        return
    if kind == 'exfat':
        # Execute the unmodified open-source exFAT writer in a separate Python
        # process. Its complete pinned source/licence ZIP remains in the cache.
        with tempfile.TemporaryDirectory(prefix='encore-exfat-', dir=output.parent) as temporary:
            work = Path(temporary)
            names = {'__init__.py', '_exfat_upcase.py', 'exfat_writer.py', 'exfat.py', 'pbar.py', 'utils.py'}
            extracted = set()
            with zipfile.ZipFile(backend) as z:
                prefix = 'ps5-ultrapack-' + SOURCE_REV + '/backend/mkpfs/'
                for info in z.infolist():
                    if info.filename.startswith(prefix) and info.filename[len(prefix):] in names:
                        name = info.filename[len(prefix):]; target = work / 'mkpfs' / name
                        target.parent.mkdir(exist_ok=True)
                        with z.open(info) as source, target.open('xb') as dest: shutil.copyfileobj(source, dest)
                        extracted.add(name)
            if extracted != names:
                raise ValueError('The exFAT tool archive is incomplete.')
            run_process(helper_command('exfat', work, app, output), cancel, log)
        return
    param = json.loads((app / 'sce_sys/param.json').read_text(encoding='utf-8'))
    env = dict(os.environ, DOTNET_SYSTEM_GLOBALIZATION_INVARIANT='1',
               DOTNET_CLI_TELEMETRY_OPTOUT='1', DOTNET_NOLOGO='1')
    with tempfile.TemporaryDirectory(prefix='encore-pkg-', dir=output.parent) as temporary:
        work = Path(temporary)
        run_process([backend, 'build', app, work, '--content-id', param['contentId'],
                     '--title-id', param['titleId'], '--title', 'CH: Encore', '--version', param['contentVersion'],
                     '--deterministic', '--no-fake-sign', '--no-retail-normalize', '--no-ampr-index'], cancel, log, env)
        files = list(work.glob('*.pkg'))
        if len(files) != 1:
            raise ValueError('The package writer did not produce exactly one PKG.')
        # Nonzero process status is necessary but the writer also reports its
        # structural validation result as JSON; explicitly reject failures.
        validation = json.loads(run_process([backend, 'validate', files[0], '--json'], cancel, log, env))
        if not isinstance(validation.get('fail'), int) or validation['fail']:
            raise ValueError('Native package validation failed.')
        files[0].replace(output)

def sys_python():
    import sys
    return sys.executable

def helper_command(kind, *args):
    import sys
    if getattr(sys, 'frozen', False):
        return [sys.executable, '--internal-' + kind, *args]
    return [sys.executable, HERE / ('exfat_backend.py' if kind == 'exfat' else 'validate_pkg.py'), *args]
