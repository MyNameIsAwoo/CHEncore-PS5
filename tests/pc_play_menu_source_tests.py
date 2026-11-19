#!/usr/bin/env python3
"""Compare the shipped option inventories to the owned PC scene, not UI guesses."""
import json,re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
scene=json.loads((root/'docs/pc-gameplay-menu-scene.json').read_text())
header=(root/'examples/cloneheronative/gameplay/play_menus.hpp').read_text()
code=(root/'examples/cloneheronative/cloneheronative.cpp').read_text()
def array(source,name):
 m=re.search(r'\b'+name+r'\[\]\s*=?(?:\s*)\{([^}]+)\}',source)
 assert m,name
 return re.findall(r'"([^"]*)"',m[1])
menus=scene['serialized_menus']
texts=lambda key:[x['text'] for x in menus[key]['strings']]
assert array(header,'quickplay')==texts('PauseMenu')[:7]
assert array(header,'practice')==texts('PauseMenu')[7:17]
for row,pc in [('optionRows','InGameSettingsMenu'),('volumeRows','VolumeMenu'),('calibrationRows','CalibrationMenu'),('songRows','SongSettingsMenu')]:
 actual=array(code,row);expected=texts(pc)
 # Encore adds the separately requested miss-sound slider after the PC mixer.
 if row=="volumeRows":expected=expected+["Miss Sound Volume"]
 assert actual==expected,(row,actual,expected)
assert array(header,'confirm')==['Ready','Instrument','Difficulty','Modifiers']
# Start/pause behavior is exercised by the native menu regression; mapped
# Start inputs extend that behavior without changing the PC option inventories.
assert 'ps5.optionsEnds = false' in code
print('PASS PC pause/practice/settings inventories; native Start is pause, not title exit')
# The player's runtime layout uses active children, not the serialized four-slot
# editor positions. Preserve the original layout-group and tint evidence.
menu_scene=json.loads((root/'docs/pc-main-menu-scene.json').read_text())
def find(node,name):
 if node['name']==name:return node
 for child in node.get('children',[]):
  result=find(child,name)
  if result:return result
canvas=menu_scene['additional_roots'][0]
group=find(canvas,'Player Menus')
layout=next(c['layout'] for c in group['components'] if c['type']=='HorizontalLayoutGroup')
assert layout['child_alignment']==4 and layout['spacing']==370
page=find(canvas,'Instrument Page')
def color(node):return next(c['color'] for c in node['components'] if c['type']=='Image')
body=color(next(c for c in page['children'] if c['name']=='LowerBG'))
header=color(next(c for c in page['children'] if c['name']=='Header'))
assert abs(body['a']-70/255)<1e-6 and body['r']==body['g']==body['b']==0
assert abs(header['r']-27/255)<1e-6 and header['a']==1
selection=color(next(c for c in page['children'] if c['name']=='Selection'))
assert all(abs(selection[k]-n/255)<1e-6 for k,n in zip(('r','g','b','a'),(208,212,214,255)))
print('PASS PC centered layout group, panel/header opacity and selection tint')
