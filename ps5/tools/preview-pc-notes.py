#!/usr/bin/env python3
"""Offline composition proof using extracted PC geometry; not a game screenshot."""
import json
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[2]
m=json.loads((ROOT/'assets/pc_guitar/manifest.json').read_text())
atlas=Image.open(ROOT/'assets/pc_guitar/guitar_atlas.png').convert('RGBA')
out=Image.new('RGBA',(1200,530),(22,25,33,255));draw=ImageDraw.Draw(out)
draw.text((25,15),'PC source sprites + extracted geometry (offline composition)',fill='white')

def compose(layer, origin, tint, scale=1000, sprite_id=None, dy=0):
    sid=sprite_id or layer['sprite_id'];s=m['sprites'][str(sid)];x,y,w,h=s['atlas_rect']
    im=atlas.crop((x,y,x+w,y+h))
    # The selected note's child transforms are identity; verify before using a frame.
    if sprite_id:
        assert all(t['m_LocalScale']==dict(x=1.,y=1.,z=1.) and
                   all(v==0 for v in t['m_LocalPosition'].values()) for t in layer['transform_chain'])
    vertices=s['vertices'] if sprite_id else layer['vertices']
    if sprite_id:
        vertices=[[-v[0] if layer['flip_x'] else v[0], -v[1] if layer['flip_y'] else v[1], v[2]] for v in vertices]
    if layer['flip_x']: im=im.transpose(Image.Transpose.FLIP_LEFT_RIGHT)
    if layer['flip_y']: im=im.transpose(Image.Transpose.FLIP_TOP_BOTTOM)
    x0=min(v[0] for v in vertices);x1=max(v[0] for v in vertices)
    y0=min(v[1] for v in vertices);y1=max(v[1] for v in vertices)
    c=[layer['color'][a] for a in 'rgba']
    if max(c[:3])-min(c[:3])>.1:c[:3]=tint
    channels=im.split()
    im=Image.merge('RGBA',tuple(channels[i].point(lambda value:round(value*c[i])) for i in range(4)))
    im=im.resize((max(1,round((x1-x0)*scale)),max(1,round((y1-y0)*scale))),Image.Resampling.LANCZOS)
    out.alpha_composite(im,(round(origin[0]+x0*scale),round(origin[1]-(y1+dy)*scale)))

for lane in range(5):
    receptor=m['receptors'][lane];tint=[next(l for l in receptor['layers'] if l['role']=='cover')['color'][a] for a in 'rgb']
    origin=(130+235*lane,235)
    for role in ['body','animation','head']:
        l=m['note_layers'][role]
        compose(l,origin,tint,scale=900*m['renderer']['scale_x'],
                sprite_id=m['roles']['body_frames'][0] if role=='animation' else None)
    for held,base_y in [(False,350),(True,455)]:
        for l in receptor['layers']:
            if (held if l['role']=='head_light' else l['enabled']):
                dy=-m['binary_evidence']['held_head_travel'] if held and l['role'] in ['head','head_cover','head_light'] else 0
                compose(l,(origin[0],base_y),tint,dy=dy)
draw.text((25,270),'Idle receptors',fill='white');draw.text((25,380),'Held receptors',fill='white')
p=ROOT/'docs/pc-guitar-composition.png';out.convert('RGB').save(p)
print(p)
