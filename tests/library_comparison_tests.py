#!/usr/bin/env python3
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Chord comparison ignores storage order, retaining flags and runtime failures."""
import importlib.util,json,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('sweep',ROOT/'tools/pc-parity/library_sweep.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='ch-comparison-') as directory:
 p=Path(directory)
 fields=['time','frets','up','down','sp','score','streak','hits','solo','spTicks','spActive','ordinary','fretBonus','health']
 row=dict.fromkeys(fields,0);row['health']=.5
 pc=dict(parsed=dict(engine={'baseScore':100},notes=[dict(time=0,mask=32,flags=0x200003),dict(time=0,mask=8,flags=0x400003)]),frames=[row])
 m.write(p/'pc.json',pc);m.write(p/'request.json',dict(instrument=3,source_hash='synthetic',frames=[row]));m.write(p/'build.json',dict(eboot_sha256='synthetic'))
 def compare(notes,score=0):
  native=dict(row,score=score)
  text='# base_score 100\n'+''.join(f'# drum {i} 0 {mask} {flags}\n' for i,(mask,flags) in enumerate(notes))
  text+='\t'.join(fields)+'\n'+'\t'.join(str(native[k]) for k in fields)+'\n';(p/'native.tsv').write_text(text)
  return m.compare(p)
 assert compare([(8,0x400000),(32,0x200000)])['status']=='pass'
 assert 'parsed_count' in compare([(32,0x200000)])['difference_counts']
 assert 'parsed_drum' in compare([(8,0x400000),(32,0x204000)])['difference_counts']
 assert 'score' in compare([(8,0x400000),(32,0x200000)],score=1)['difference_counts']
 print('PASS unordered chord contents; missing gems, roll flags and score differences remain failures')
