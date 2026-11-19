#!/usr/bin/env python3
# Copyright (C) 2026 HOWL
# SPDX-License-Identifier: MIT
"""Render CH: Encore typography with the Apache-2.0 Roboto font; no PC artwork."""
from pathlib import Path
import argparse,struct,shutil
from PIL import Image,ImageDraw,ImageFont
import ispc_texcomp
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--font',type=Path,required=True);a=p.parse_args()
 out=ROOT/'ps5/assets/ch-branding';out.mkdir(parents=True,exist_ok=True)
 bg=(5,25,42,255);ink=(244,248,252,255);muted=(172,193,210,255)
 def label(im,text,position,size,color=ink):ImageDraw.Draw(im).text(position,text,font=ImageFont.truetype(str(a.font),size),fill=color,anchor='mm')
 icon=Image.new('RGBA',(512,512),bg);label(icon,'CH',(256,210),264);label(icon,'ENCORE',(256,390),63);icon.save(out/'icon0.png')
 logo=Image.new('RGBA',(1100,600));label(logo,'CH',(550,205),420);label(logo,'ENCORE',(550,505),126);logo.save(out/'menu-logo.png')
 splash=Image.new('RGBA',(3840,2160),bg);label(splash,'CH: ENCORE',(1920,970),240);label(splash,'v1.5.0  /  pursuaded',(1920,1210),68,muted);splash.save(out/'splash.png')
 for filename,w,h,body in [('icon.svg',512,512,'<rect width="512" height="512" fill="#05192a"/><text x="256" y="285" font-size="264">CH</text><text x="256" y="410" font-size="63">ENCORE</text>'),('menu-logo.svg',1100,600,'<text x="550" y="335" font-size="420">CH</text><text x="550" y="540" font-size="126">ENCORE</text>')]:
  (out/filename).write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}"><g font-family="Roboto" text-anchor="middle" fill="#f4f8fc">{body}</g></svg>\n')
 surface=ispc_texcomp.RGBASurface(splash.tobytes(),3840,2160);blocks=ispc_texcomp.compress_blocks_bc7(surface,ispc_texcomp.BC7EncSettings.from_profile('slow'))
 header=b'DDS '+struct.pack('<7I',124,0xA1007,2160,3840,len(blocks),0,1)+bytes(44)+struct.pack('<8I',32,4,int.from_bytes(b'DX10','little'),0,0,0,0,0)+struct.pack('<5I',0x1000,0,0,0,0)+struct.pack('<5I',98,3,0,1,0)
 assert len(header)==148
 for name in ('pic0.dds','pic1.dds'):(out/name).write_bytes(header+blocks)
 (out/'LICENSE.txt').write_text('CH: Encore typographic artwork: Copyright (C) 2026 HOWL, MIT.\nRendered using Roboto Medium, Copyright Christian Robertson / Google, Apache-2.0.\nNo original PC game artwork is included.\n')
 print('Created CH: Encore typography and shell artwork from the licensed Roboto font.')
if __name__=='__main__':main()
