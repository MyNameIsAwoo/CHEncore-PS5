#!/usr/bin/env python3
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Independent MIDI fixtures for six frets and tap phrase boundary/duration."""
import importlib.util,re,struct,subprocess,tempfile
from pathlib import Path
spec=importlib.util.spec_from_file_location('packages',Path(__file__).with_name('song_package_tests.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory(prefix='encore-six-midi-') as tmp:
 root=Path(tmp)
 for six in (False,True):
  name=b'PART GUITAR GHL' if six else b'PART GUITAR'
  for marker in (False,True):
   events=[(0,b'\xff\x03'+bytes([len(name)])+name)]
   for i,tick in enumerate((100,200,300,400,500,600)):
    m.note(events,100 if six and tick==500 else (95+i%3 if six else 96+i%3),tick,tick+200)
   # A separate Hard note inside an Expert-only SysEx phrase must stay normal.
   m.note(events,83 if six else 84,550,551)
   for tick,state in ((400,1),(600,0)):
    payload=b'PS\0\0'+bytes([3,4,state])+b'\xf7';events.append((tick,b'\xf0'+m.vlq(len(payload))+payload))
   if marker:m.note(events,104,200,500)
   if six:
    m.note(events,103,0,150);m.note(events,103,650,750)
   data=b'MThd'+struct.pack('>IHHH',6,0,1,192)+m.track(events)
   inp=root/'notes.mid';out=root/'notes.chart';inp.write_bytes(data)
   subprocess.run([str(m.PROBE),'midi',str(inp),str(out)],check=True,capture_output=True)
   chart=out.read_text();suffix='GHLGuitar' if six else 'Single'
   def section(level):return re.search(r'\['+level+suffix+r'\]\s*\{(.*?)\}',chart,re.S).group(1)
   expert=section('Expert');tapTicks={int(x) for x in re.findall(r'(\d+) = N 6 0',expert)}
   assert tapTicks==({200,300,400,500} if marker else {400,500}),(six,marker,tapTicks)
   assert 'N 6 0' not in section('Hard')
   if six:
    assert '500 = N 8 200' in expert,expert
    assert expert.count('E solo\n')==2 and 'S 2 ' not in expert
print('PASS five/six-fret tap starts, exclusive endpoints, overlapping note/SysEx ranges, difficulty scope, sixth-fret tails and six-fret solos')
