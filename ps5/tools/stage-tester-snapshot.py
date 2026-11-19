#!/usr/bin/env python3
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Freeze a private buildable tester snapshot without changing the working tree.

Use the previous audited snapshot as the asset/vendor allowlist, take current
runtime/build/test sources, and create a new local-only clean repository.
No logs, credentials, console data or downloaded user songs are copied.
"""
import argparse,hashlib,json,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
p=argparse.ArgumentParser();p.add_argument('--seed',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
seed=a.seed.resolve();out=a.out.resolve();assert not out.exists(),f'Refusing to replace {out}'
files=set(subprocess.check_output(['git','-C',str(seed),'ls-files','-z']).decode().split('\0'));files.discard('')
# Runtime additions since the previous release. Vendor/assets remain restricted
# to the audited seed list; their current contents are used where present.
for folder in ('base','examples/cloneheronative','ps5','tests','shaders'):
 for path in (ROOT/folder).rglob('*'):
  if path.is_file() and not path.is_symlink() and path.suffix.lower() in ('.c','.cpp','.h','.hpp','.inc','.py','.sh','.cmake','.txt','.md','.json','.vert','.frag','.comp','.spv','.png','.dds','.crt'):
   if any(part in ('__pycache__','.git','.deps','build','dist','klog','screenshots') for part in path.relative_to(ROOT).parts):continue
   files.add(path.relative_to(ROOT).as_posix())
for path in (ROOT/'assets/controller_profiles').iterdir():
 if path.is_file() and path.suffix in ('.json','.txt'):files.add(path.relative_to(ROOT).as_posix())
for name in ('docs/input-transport-audit.md','docs/beta-diagnostics.md','docs/controller-catalog.md','docs/alpha36-solo-records.md','docs/alpha34-engine-verification.md','docs/ps5-audio-playback.md','docs/pc-input-timing-audit.md','docs/switch-performance-checklist.md'):files.add(name)
files.update(('.gitignore','README.md','LICENSE.md'))
files={name for name in files if not any(part in ('.env','test-run.txt','test-results.txt','__pycache__','screenshots','klog','userdata','pc_bundled_songs','pc_menu','pc_guitar','pc_drums','pc_guitar_fx','pc_gameplay_hud','pc_score_hud','pc_highway','pc_six_fret','Sprite','Texture2D','Font','AudioClip') for part in Path(name).parts)}
for name in files:
 assert not any(part in ('.env','test-run.txt','test-results.txt','__pycache__','screenshots','klog','userdata','pc_bundled_songs','pc_menu','pc_guitar','pc_drums','pc_guitar_fx','pc_gameplay_hud','pc_score_hud','pc_highway','pc_six_fret','Sprite','Texture2D','Font','AudioClip') for part in Path(name).parts),name
out.mkdir(parents=True);manifest={}
for name in sorted(files):
 source=ROOT/name
 if not source.is_file():continue
 dest=out/name;dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(source,dest)
 manifest[name]=hashlib.sha256(dest.read_bytes()).hexdigest()
param=json.loads((out/'ps5/sce_sys/param.json').read_text())
version=int(param['contentVersion'].split('.')[-1])
songs=[];assert not (out/'assets/pc_bundled_songs').exists()
subprocess.run(['git','init','-b','main',str(out)],check=True,stdout=subprocess.DEVNULL)
subprocess.run(['git','-C',str(out),'add','.'],check=True)
subprocess.run(['git','-C',str(out),'-c','user.name=HOWL','-c','user.email=howl@localhost','commit','-m',f'Freeze Alpha v.{version} private tester source'],check=True,stdout=subprocess.DEVNULL)
assert not subprocess.check_output(['git','-C',str(out),'status','--porcelain'])
revision=subprocess.check_output(['git','-C',str(out),'rev-parse','HEAD'],text=True).strip()
(out/'.deps').mkdir();(out/'.deps/native').symlink_to(ROOT/'.deps/native',target_is_directory=True)
record=dict(status='pass',source=str(out),revision=revision,files=len(manifest),songs=songs,sha256=manifest,public_upload=False)
(ROOT/f'build/alpha{version}-snapshot-manifest.json').write_text(json.dumps(record,indent=2)+'\n')
print(json.dumps({k:record[k] for k in ('status','source','revision','files','public_upload')}),flush=True)
