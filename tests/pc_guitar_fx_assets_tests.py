#!/usr/bin/env python3
"""Validate unchanged extracted pixels and the recovered guitar effect inventory."""
from pathlib import Path
import hashlib,json,math
from PIL import Image
root=Path(__file__).resolve().parents[1]/'assets/pc_guitar_fx'
m=json.loads((root/'manifest.json').read_text(),parse_constant=lambda x:(_ for _ in ()).throw(ValueError(x)))
atlas=Image.open(root/'guitar_fx_atlas.png').convert('RGBA')
assert list(atlas.size)==m['atlas_size']
for sprite in m['sprites'].values():
 x,y,w,h=sprite['atlas_rect']
 assert hashlib.sha256(atlas.crop((x,y,x+w,y+h)).tobytes()).hexdigest()==sprite['pixels_sha256']
for name in ('flames','lightning','lightning2','sparks','sparksOverlay','spSparks'):
 assert len(m['effects'][name])==5
 for animation in m['effects'][name]:
  assert animation['frames'] and animation['interval']>0
  assert animation['layer']['key'][0]==14
  assert animation['layer']['blend']['dst']==1
  assert all(math.isfinite(x) for point in animation['layer']['vertices'] for x in point)
for name in ('noteParticles','sustainParticles'):
 assert len(m['particles'][name])==5
 for emitter in m['particles'][name]:
  assert emitter['life']>0 and emitter['size']>0 and emitter['speed']>0
  assert emitter['key'][0]==14 and emitter['renderer']['m_RenderMode']==0
assert len(m['openfx'])==2
# The separate overlay is started with Animator's no-color overload, whereas
# the spark layer receives ColorProfile +0x344/+0x348 (Ghidra 0x180090430).
for animation in m['effects']['sparksOverlay']:
 assert animation['normal']==animation['active']==[animation['layer']['color'][a] for a in 'rgba']==[1,1,1,1]
print('PASS original pixel hashes for',len(m['sprites']),'sprites/textures, all six animation families, open effects and ten source emitters')
