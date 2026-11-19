#!/usr/bin/env python3
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Reject PC resources and user data in tracked source or release folders."""
import argparse,hashlib,subprocess,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def forbidden(name):
 p=Path(name);parts=p.parts
 if any(x in ('pc-assets','pc_bundled_songs','userdata','.env') for x in parts):return True
 if name.startswith(('assets/pc_','assets/Sprite/','assets/Texture2D/','assets/Font/','assets/AudioClip/','assets/songs/','songs/','ps5/assets/launcher/','docs/images/main-menu')):return True
 if p.suffix.lower() in ('.encore-assets','.srb','.sng','.ress','.assets'):return True
 if p.name in ('GameAssembly.dll','import-pc-bundled-songs.py'):return True
 return False

def check(folder=None):
 lines=(ROOT/'tools/asset-importer/excluded-resource-hashes.txt').read_text().splitlines();hashes={x for x in lines if x and not x.startswith('#')}
 if folder:
  paths=[(p.relative_to(folder).as_posix(),p) for p in folder.rglob('*') if p.is_file()]
 else:
  names=subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode().split('\0')
  paths=[(n,ROOT/n) for n in names if n and (ROOT/n).is_file()]
 errors=[]
 for name,p in paths:
  if forbidden(name):errors.append(name+': excluded resource path');continue
  if p.suffix.lower() in ('.png','.jpg','.jpeg','.dds','.ttf','.opus','.wav','.ogg','.json'):
   h=hashlib.sha256(p.read_bytes()).hexdigest()
   if h in hashes:errors.append(name+': matches an excluded original resource')
 if errors:
  print('\n'.join(errors),file=sys.stderr);raise SystemExit(1)
 print('PASS no excluded PC resources or user data: '+str(len(paths))+' files')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--app',type=Path);a=p.parse_args();check(a.app)
