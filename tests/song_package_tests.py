#!/usr/bin/env python3
# Copyright (C) 2026 Mihawk
# SPDX-License-Identifier: MIT
"""Exercise transactional song packages with independent ZIP/SMF writers."""
import io,os,struct,subprocess,tempfile,zipfile,json
from pathlib import Path
PROBE=Path('/tmp/song_package_probe')
def vlq(n):
 b=[n&127];n>>=7
 while n:b.insert(0,(n&127)|128);n>>=7
 return bytes(b)
def track(events):
 data=b'';last=0
 for tick,event in sorted(events,key=lambda x:x[0]):data+=vlq(tick-last)+event;last=tick
 data+=b'\0\xff\x2f\0'
 return b'MTrk'+struct.pack('>I',len(data))+data
def note(events,pitch,start,end):events.extend([(start,bytes([0x90,pitch,100])),(end,bytes([0x90,pitch,0]))])
def midi():
 tempo=track([(0,b'\xff\x51\x03\x07\xa1\x20'),(0,b'\xff\x58\x04\x04\x02\x18\x08'),(960,b'\xff\x51\x03\x06\x1a\x80')])
 events=[(0,b'\xff\x03\x0bPART GUITAR'),(0,b'\xff\x01\x10[ENHANCED_OPENS]')]
 for p,a,b in [(96,0,100),(97,120,180),(102,120,200),(98,480,580),(101,480,600),(99,720,820),(104,700,850),(95,960,1060),(116,0,1200),(103,0,240),(100,1440,1920),(96,2100,2260),(97,2400,2561),(98,2700,2862)]:note(events,p,a,b)
 drums=[(0,b'\xff\x03\x0aPART DRUMS')]
 for p,a,b in [(96,0,1),(97,120,121),(98,240,241),(110,200,300),(116,0,480),(126,100,400),(120,480,960)]:note(drums,p,a,b)
 return b'MThd'+struct.pack('>IHHH',6,1,3,480)+tempo+track(events)+track(drums)
CHART=b'[Song]\n{\n Resolution = 192\n}\n[SyncTrack]\n{\n 0 = B 120000\n}\n[ExpertSingle]\n{\n 0 = N 0 0\n}\n'
def sng(files):
 metadata=struct.pack('<Q',0)
 indexSize=8+sum(1+len(n.encode())+16 for n in files)
 dataStart=26+8+len(metadata)+8+indexSize+8
 index=struct.pack('<Q',len(files));body=b''
 key=bytes(range(16))
 for n,data in files.items():
  name=n.encode();index+=bytes([len(name)])+name+struct.pack('<QQ',len(data),dataStart+len(body))
  body+=bytes(v^key[i&15]^(i&255) for i,v in enumerate(data))
 return b'SNGPKG'+struct.pack('<I',1)+key+struct.pack('<Q',len(metadata))+metadata+struct.pack('<Q',len(index))+index+struct.pack('<Q',len(body))+body
class Nonseek(io.BytesIO):
 def seekable(self):return False
 def seek(self,*args):raise io.UnsupportedOperation()
def zipbytes(files,method=zipfile.ZIP_DEFLATED,descriptor=False):
 data=Nonseek() if descriptor else io.BytesIO()
 with zipfile.ZipFile(data,'w',compression=method) as z:
  for name,content in files.items():z.writestr(name,content)
 return data.getvalue()
def run():
 verdict=[]
 with tempfile.TemporaryDirectory(prefix='ch-packages-') as tmp:
  root=Path(tmp)
  def package(name,data,success=True,cancel=False):
   path=root/(name+'.package');path.write_bytes(data);stage=root/(name+'.stage');dest=root/(name+'.song')
   cmd=[str(PROBE),'package',str(path),str(stage),str(dest)]+(['cancel'] if cancel else [])
   p=subprocess.run(cmd,capture_output=True,text=True)
   assert (p.returncode==0)==success,(name,p.returncode,p.stderr)
   assert dest.exists()==success,(name,'partial destination')
   if success:assert (dest/'notes.chart').exists(),name
   verdict.append(name);return dest
  large=os.urandom(180000)+b'A'*200000
  for method in [zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED]:
   dest=package('zip-'+str(method),zipbytes({'wrapper/song/notes.chart':CHART,'wrapper/song/song.ogg':large},method))
   assert (dest/'song.ogg').read_bytes()==large
  package('descriptor',zipbytes({'notes.chart':CHART},descriptor=True))
  package('windows-path',zipbytes({'wrapper\\notes.chart':CHART}))
  package('sng-chart',sng({'notes.chart':CHART,'song.ogg':large}))
  package('sng-midi',sng({'notes.mid':midi()}))
  gh=[(0,b'\xff\x03\x0aPART DRUMS')]
  for pitch,tick in [(65,0),(98,192),(99,384),(100,576),(101,768)]:note(gh,pitch,tick,tick+1)
  gh=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(gh)
  ghSong=package('midi-five-lane-pads',zipbytes({'notes.mid':gh}))
  ghChart=(ghSong/'notes.chart').read_text()
  assert '384 = N 3 0' in ghChart and '384 = N 67' not in ghChart
  assert '576 = N 68 0' in ghChart and '768 = N 5 0' in ghChart
  assert '192 = N 66 0' in ghChart
  configured=[(0,b'\xff\x03\x0aPART DRUMS')];note(configured,99,192,193)
  configured=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(configured)
  forced=package('midi-five-lane-ini',zipbytes({'notes.mid':configured,'song.ini':b'[song]\nfive_lane_drums = True\n'}))
  assert '192 = N 67' not in (forced/'notes.chart').read_text()
  collision=[(0,b'\xff\x03\x0aPART DRUMS')]
  for pitch in (96,100,101):note(collision,pitch,192,193)
  collision=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(collision)
  folded=package('midi-five-lane-chord',zipbytes({'notes.mid':collision}))
  foldedChart=(folded/'notes.chart').read_text()
  assert '192 = N 3 0' in foldedChart and '192 = N 4 0' in foldedChart
  assert '192 = N 68 0' in foldedChart and '192 = N 5 0' not in foldedChart
  plus=[(0,b'\xff\x03\x0aPART DRUMS')];note(plus,95,192,193);note(plus,96,384,385)
  plus=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(plus)
  separated=package('midi-expert-plus-kick',zipbytes({'notes.mid':plus}))
  plusChart=(separated/'notes.chart').read_text()
  assert '192 = N 32 0' in plusChart and '192 = N 0 0' not in plusChart
  assert '384 = N 0 0' in plusChart
  vocals=[(0,b'\xff\x03\x0bPART VOCALS'),(120,b'\xff\x05\x05Hello'),(240,b'\xff\x01\x06world#'),(300,b'\xff\x01\x08[ignore]')]
  note(vocals,105,96,480)
  vocalMidi=midi()
  vocalMidi=vocalMidi[:10]+struct.pack('>H',4)+vocalMidi[12:]+track(vocals)
  vocalSong=package('midi-vocal-lyrics',zipbytes({'notes.mid':vocalMidi}))
  vocalChart=(vocalSong/'notes.chart').read_text()
  assert '96 = E "phrase_start"' in vocalChart and '480 = E "phrase_end"' in vocalChart
  assert '120 = E "lyric Hello"' in vocalChart and '240 = E "lyric world#"' in vocalChart
  assert 'lyric [ignore]' not in vocalChart
  # Dynamics is explicitly enabled at a chart time. Earlier soft notes and
  # ordinary velocities stay unmarked; both pad/cymbal dynamics survive import.
  dynamics=[(0,b'\xff\x03\x0aPART DRUMS'),
            (300,b'\xff\x01'+vlq(len('[ENABLE_CHART_DYNAMICS]'))+b'[ENABLE_CHART_DYNAMICS]')]
  for pitch,tick,velocity in [(97,192,1),(98,384,127),(99,576,1),(100,768,90)]:
   dynamics.extend([(tick,bytes([0x90,pitch,velocity])),(tick+1,bytes([0x80,pitch,0]))])
  dynamics=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(dynamics)
  dynamicSong=package('midi-dynamics',zipbytes({'notes.mid':dynamics}))
  dynamicChart=(dynamicSong/'notes.chart').read_text()
  assert '192 = N 40 0' not in dynamicChart and '384 = N 35 0' in dynamicChart
  assert '576 = N 42 0' in dynamicChart and '768 = N 37 0' not in dynamicChart
  # A PS Open phrase folds a same-tick two-fret chord into one Open head.
  phrase=b'PS\0\0\xff\x01'
  opened=[(0,b'\xff\x03\x0bPART GUITAR'),(180,b'\xf0\x08'+phrase+b'\x01\xf7'),(220,b'\xf0\x08'+phrase+b'\x00\xf7')]
  note(opened,96,192,384);note(opened,97,192,384)
  opened=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(opened)
  openSong=package('midi-open-chord',zipbytes({'notes.mid':opened}))
  assert (openSong/'notes.chart').read_text().count('192 = N 7 192')==1
  # Custom INI thresholds survive moving the converted chart into the writable
  # cache, away from the read-only USB source and its INI file.
  custom=root/'custom-midi';custom.mkdir();(custom/'notes.mid').write_bytes(midi())
  (custom/'song.ini').write_text('[song]\nhopo_frequency = 66\n')
  p=subprocess.run([str(PROBE),'midi',str(custom/'notes.mid'),str(root/'custom.chart')],capture_output=True)
  assert p.returncode==0 and 'MidiHopoThreshold = 66' in (root/'custom.chart').read_text()
  verdict.append('midi-custom-threshold-cache')
  # Only this converter generation's metadata can be reused. Old imports,
  # authored charts and markers in note sections must trigger a rebuild.
  current=(root/'custom.chart').read_bytes()
  assert b'MidiImportVersion = 5' in current
  for label,data,valid in [('current',current,True),
    ('old',current.replace(b'MidiImportVersion = 5',b'MidiImportVersion = 1'),False),
    ('legacy',current.replace(b'  MidiImportVersion = 5\n',b''),False),
    ('authored',current.replace(b'MidiImport = 1',b'MidiImport = 0'),False),
    ('wrong-section',b'[ExpertSingle]\n{\n MidiImport = 1\n MidiImportVersion = 5\n}\n',False),
    ('truncated',b'[Song]\n{\n MidiImport = 1\n MidiImportVersion = 5\n',False),
    ('bounded',b'[Song]\n{\n'+b' Name = x\n'*70+b' MidiImport = 1\n MidiImportVersion = 5\n}\n',False),
    ('crlf',current.replace(b'\n',b'\r\n'),True)]:
   check=root/('cache-'+label+'.chart');check.write_bytes(data)
   assert (subprocess.run([str(PROBE),'cache',str(check)],capture_output=True).returncode==0)==valid,label
  verdict.append('midi-versioned-cache')
  # The PC MIDI reader keeps [end] only as MIDI metadata; an equivalent
  # chart text event does not cap playback. Keep valid source end markers
  # separately, and ignore markers preceding last-note-end + one quarter.
  for endTick,valid in [(3000,False),(4000,True)]:
   source=bytearray(midi());source[11]=4
   marker=[(0,b'\xff\x03\x06EVENTS'),(endTick,b'\xff\x01\x05[end]')]
   source+=track(marker)
   src=root/('end-'+str(endTick)+'.mid');src.write_bytes(source);dst=root/('end-'+str(endTick)+'.chart')
   p=subprocess.run([str(PROBE),'midi',str(src),str(dst)],capture_output=True)
   assert p.returncode==0 and ('MidiEndTick' in dst.read_text())==valid
  verdict.append('midi-end-marker-cache')
  # Drum pitch 103 always represents a solo, including multiple solos without
  # a 116 phrase. The legacy guitar fallback must not turn these into SP.
  soloNotes=[(0,b'\xff\x03\x0aPART DRUMS')]
  for pitch,start,end in [(103,0,192),(97,96,97),(103,384,576),(98,480,481)]:note(soloNotes,pitch,start,end)
  drumSolos=b'MThd'+struct.pack('>IHHH',6,0,1,192)+track(soloNotes)
  soloSong=package('midi-multiple-drum-solos',zipbytes({'notes.mid':drumSolos}))
  soloChart=(soloSong/'notes.chart').read_text()
  assert soloChart.count('E solo\n')==2 and 'S 2 ' not in soloChart
  legacy=[(0,b'\xff\x03\x0bPART GUITAR')]
  for pitch,start,end in [(103,0,192),(96,48,49),(97,144,145),(103,384,576),(98,432,433),(99,528,529)]:note(legacy,pitch,start,end)
  legacy=b'MThd'+struct.pack('>IHHH',6,1,2,192)+track([(0,b'\xff\x51\x03\x07\xa1\x20')])+track(legacy)
  legacySong=package('midi-legacy-guitar-phrases',zipbytes({'notes.mid':legacy}))
  legacyChart=(legacySong/'notes.chart').read_text()
  assert legacyChart.count('S 2 192')==2 and 'E solo' not in legacyChart
  # PC track reader keeps the last channel status across PS SysEx and meta events.
  events=(b'\x00\xff\x03\x0bPART GUITAR'+b'\x00\x90\x60\x64'+
          b'\x00\xf0\x08PS\x00\x00\xff\x04\x01\xf7'+vlq(192)+b'\x60\x00'+
          b'\x00\xf0\x08PS\x00\x00\xff\x04\x00\xf7'+b'\x00\x61\x64'+
          b'\x00\xff\x01\x04test'+vlq(192)+b'\x61\x00'+b'\x00\xff\x2f\x00')
  continued=b'MThd'+struct.pack('>IHHH',6,0,1,192)+b'MTrk'+struct.pack('>I',len(events))+events
  running=package('running-after-sysex-meta',zipbytes({'notes.mid':continued}))
  chart=(running/'notes.chart').read_text();assert '0 = N 0 192' in chart and '192 = N 1 192' in chart
  bad=b'\x00\xff\x03\x0bPART GUITAR\x00\x60\x64\x00\xff\x2f\x00'
  bad=b'MThd'+struct.pack('>IHHH',6,0,1,192)+b'MTrk'+struct.pack('>I',len(bad))+bad
  package('running-without-initial-channel',zipbytes({'notes.mid':bad}),False)
  dest=package('zip-midi',zipbytes({'song/notes.midi':midi(),'song/song.ini':b'[song]\nname = Test\n'}))
  text=(dest/'notes.chart').read_text();assert 'Resolution = 480' in text and '960 = B 150000' in text
  assert '[EasySingle]' not in text and '[EasyDrums]' not in text
  upper=package('upper-ini',zipbytes({'notes.mid':midi(),'SONG.INI':b'[song]\nname = Original metadata\n'}))
  assert (upper/'song.ini').read_text()=='[song]\nname = Original metadata\n'
  guitar=text.split('[ExpertSingle]')[1]
  assert '120 = N 5 0' in guitar and '480 = N 5 0' in guitar and '720 = N 6 0' in guitar and '960 = N 7 0' in guitar
  assert '0 = S 2 1200' in guitar and '240 = E soloend' in guitar
  assert '2100 = N 0 0' in guitar and '2400 = N 1 0' in guitar and '2700 = N 2 162' in guitar
  drums=text.split('[ExpertDrums]')[1].split('}\n')[0]
  assert '240 = N 66 0' not in drums and '100 = S 65 300' in drums and '480 = S 64 480' in drums
  package('traversal',zipbytes({'../notes.chart':CHART}),False)
  package('absolute',zipbytes({'/notes.chart':CHART}),False)
  package('duplicate-case',zipbytes({'notes.chart':CHART,'NOTES.CHART':CHART}),False)
  package('two-songs',zipbytes({'a/notes.chart':CHART,'b/notes.chart':CHART}),False)
  package('no-chart',zipbytes({'readme.txt':b'no chart'}),False)
  package('truncated',zipbytes({'notes.chart':CHART})[:-8],False)
  package('unsupported-compression',zipbytes({'notes.chart':CHART},zipfile.ZIP_BZIP2),False)
  data=io.BytesIO()
  with zipfile.ZipFile(data,'w') as z:
   info=zipfile.ZipInfo('notes.chart');info.create_system=3;info.external_attr=(0o120777<<16);z.writestr(info,b'/tmp/x')
  package('symlink',data.getvalue(),False)
  crc=bytearray(zipbytes({'notes.chart':CHART},zipfile.ZIP_STORED));at=30+len('notes.chart');crc[at]^=1
  package('crc',crc,False)
  package('cancelled',zipbytes({'notes.chart':CHART}),False,True)
  package('not-a-package',b'<!DOCTYPE html>not an archive',False)
  # A failed install cannot replace an existing library entry.
  path=root/'conflict.zip';path.write_bytes(zipbytes({'notes.chart':CHART}));stage=root/'conflict.stage';dest=root/'existing';dest.mkdir();(dest/'keep').write_text('unchanged')
  p=subprocess.run([str(PROBE),'package',str(path),str(stage),str(dest)],capture_output=True);assert p.returncode!=0 and (dest/'keep').read_text()=='unchanged';verdict.append('existing-destination')
  # Header and variable-length fields must fail without an output chart.
  for name,data in [('smpte',midi()[:12]+b'\xe7\x28'+midi()[14:]),('bad-vlq',b'MThd'+struct.pack('>IHHH',6,1,1,480)+b'MTrk'+struct.pack('>I',6)+b'\x81'*5+b'\0'),('truncated-midi',midi()[:-3])]:
   src=root/(name+'.mid');out=root/(name+'.chart');src.write_bytes(data)
   p=subprocess.run([str(PROBE),'midi',str(src),str(out)],capture_output=True);assert p.returncode!=0 and not out.exists(),name;verdict.append(name)
 print(json.dumps({'verdict':'PASS','cases':len(verdict),'names':verdict}))
if __name__=='__main__':run()
