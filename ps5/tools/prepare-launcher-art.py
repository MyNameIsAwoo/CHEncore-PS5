#!/usr/bin/env python3
"""Prepare supplied artwork for PS5. Requires Pillow and ispc_texcomp 1.0.1.

Uses the provided pixels: icon resized to 512 square; background retained at 4K
and BC7-compressed for the shell. No cropping or generated artwork.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,struct
from PIL import Image,ImageChops,ImageStat
import ispc_texcomp
ROOT=Path(__file__).resolve().parents[2]
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--icon',type=Path,required=True);p.add_argument('--background',type=Path,required=True)
 p.add_argument('--stage',action='store_true',help='Copy the outputs into the existing built title folder')
 a=p.parse_args();sources=ROOT/'ps5/assets/launcher';out=ROOT/'ps5/sce_sys'
 sources.mkdir(parents=True,exist_ok=True);out.mkdir(parents=True,exist_ok=True)
 for source,name in ((a.icon,'app-icon-source.png'),(a.background,'splash-screen-source.png')):
  target=sources/name
  if source.resolve()!=target.resolve():shutil.copyfile(source,target)
 icon=Image.open(sources/'app-icon-source.png').convert('RGBA')
 if icon.width!=icon.height:raise ValueError('Icon must be square to preserve artwork')
 icon.resize((512,512),Image.Resampling.LANCZOS).save(out/'icon0.png')
 bg=Image.open(sources/'splash-screen-source.png').convert('RGBA')
 if bg.size!=(3840,2160):raise ValueError('Background must be 3840x2160 to preserve artwork')
 surface=ispc_texcomp.RGBASurface(bg.tobytes(),bg.width,bg.height)
 blocks=ispc_texcomp.compress_blocks_bc7(surface,ispc_texcomp.BC7EncSettings.from_profile('slow'))
 assert len(blocks)==3840*2160
 header=b'DDS '+struct.pack('<7I',124,0xA1007,2160,3840,len(blocks),0,1)+bytes(44)
 header+=struct.pack('<8I',32,4,int.from_bytes(b'DX10','little'),0,0,0,0,0)
 header+=struct.pack('<5I',0x1000,0,0,0,0)+struct.pack('<5I',98,3,0,1,0)
 assert len(header)==148
 for name in ('pic0.dds','pic1.dds'):(out/name).write_bytes(header+blocks)
 decoded=Image.open(out/'pic1.dds').convert('RGBA')
 error=ImageStat.Stat(ImageChops.difference(bg,decoded)).rms
 assert max(error)<3,error
 manifest={'sources':{p.name:sha(p) for p in sources.glob('*-source.png')},
           'outputs':{n:sha(out/n) for n in ('icon0.png','pic0.dds','pic1.dds')},
           'icon_size':[512,512],'background_size':[3840,2160],
           'background_format':'BC7_UNORM DX10, one mip','compression_rms_rgba':error}
 (sources/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
 if a.stage:
  title=json.loads((ROOT/'ps5/sce_sys/param.json').read_text())['titleId']
  target=ROOT/'dist'/title/'sce_sys'
  if not (target.parent/'eboot.bin').is_file():raise ValueError('Build the native title before staging artwork')
  for name in manifest['outputs']:shutil.copyfile(out/name,target/name)
 print('Prepared current supplied artwork; DDS decode RMS:',error)
if __name__=='__main__':main()
