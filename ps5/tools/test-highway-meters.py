#!/usr/bin/env python3
"""Capture meter scaling at 50/100/200% and scrolling at 0.5/1/2x.

Run on an idle console after deploying. A temporary autoplay profile replaces
only this title's profile file during the tests; the original bytes are restored
and verified in finally. The persistent backup under build/ps5 survives an
interrupted test. Uses the title's own frame budget, results and screenshots.

Copyright (C) 2026 Mihawk
SPDX-License-Identifier: MIT
"""
import io
import json
import os
from pathlib import Path
import subprocess
import sys
from ftplib import FTP, error_perm, error_reply
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
VULKAN = Path(os.environ.get('PS5_VULKAN_DIR', ROOT.parent / 'PS5_Vulkan'))
sys.path.insert(0, str(VULKAN / 'tools'))
import ps5_console

SCRIPT = '''frames 1200
screenshot
samples cloneheronative
press cloneheronative.p1 20 options
press cloneheronative.p1 40 cross
press cloneheronative.p1 60 touchpad
press cloneheronative.p1 80 down
press cloneheronative.p1 100 down
press cloneheronative.p1 120 down
press cloneheronative.p1 140 cross
press cloneheronative.p1 160 down
press cloneheronative.p1 180 cross
'''


def main():
    os.chdir(ROOT)
    settings = ps5_console.load_settings()
    title = json.loads((ROOT / 'ps5/sce_sys/param.json').read_text())['titleId']
    base = f'/data/homebrew/{title}'
    profile = base + '/userdata/clonehero-profiles.cfg'
    backup_path = ROOT / 'build/ps5/meter-profiles-backup.cfg'
    if backup_path.exists():
        raise RuntimeError(f'Restore the profile backup at {backup_path} before another run')
    if ' count=0 ' not in ps5_console.ps5vkctl_command(settings, 'procs') + ' ':
        raise RuntimeError('Close the running title before this regression run')

    def ftp():
        client = FTP()
        client.connect(settings['host'], settings['ftp_port'], timeout=15)
        client.login(settings['ftp_user'], settings['ftp_password'] or 'codex')
        return client

    def get(path):
        data = io.BytesIO()
        with ftp() as client:
            client.retrbinary('RETR ' + path, data.write)
        return data.getvalue()

    def put(path, data):
        with ftp() as client:
            client.storbinary('STOR ' + path, io.BytesIO(data))

    backup = get(profile)
    backup_path.parent.mkdir(parents=True, exist_ok=True)
    backup_path.write_bytes(backup)
    output = ROOT / 'klog/meter-scaling'
    output.mkdir(exist_ok=True)
    try:
        for length, speed in [(100, 1.0), (50, 1.0), (200, 1.0), (100, 0.5), (100, 2.0)]:
            run = output / f'length-{length}-speed-{speed:.1f}'
            run.mkdir(exist_ok=True)
            put(profile, f'CHPS5PROFILES 3\n{speed:.1f} 0 {length} 4 0 Meter Test\n'.encode())
            put(base + '/test-run.txt', SCRIPT.encode())
            print(f'CASE length={length}% speed={speed:.1f}', flush=True)
            subprocess.run([sys.executable, 'ps5/tools/run-without-klog.py', str(run), '90'], check=True)
            result = (run / 'test-results.txt').read_text()
            if 'ends: status 0' not in result or 'samples: 1 ok, 0 failed' not in result:
                raise RuntimeError(f'Console test failed: {run}')
            ppm = run / 'cloneheronative.ppm'
            ppm.write_bytes(get(base + '/screenshots/cloneheronative.ppm'))
            Image.open(ppm).save(ppm.with_suffix('.png'))
    finally:
        put(profile, backup)
        if get(profile) != backup:
            raise RuntimeError(f'Profile restore failed; original remains at {backup_path}')
        backup_path.unlink()
        with ftp() as client:
            try:
                client.delete(base + '/test-run.txt')
            except (error_perm, error_reply):
                pass
        print('Restored and verified original saved profiles.', flush=True)


if __name__ == '__main__':
    main()
