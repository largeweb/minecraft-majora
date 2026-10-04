"""Extract only owned local sword icons; no remote assets."""
from pathlib import Path
import zipfile,struct,zlib,json
from common import ROOT, config
root=ROOT
c=config()
out=root/'.local/generated/sword-pack';out.mkdir(parents=True,exist_ok=True)
def png(w,h,rgba):
 def chunk(t,b):return struct.pack('>I',len(b))+t+b+struct.pack('>I',zlib.crc32(t+b))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>2I5B',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(b'\0'+rgba[y*w*4:(y+1)*w*4] for y in range(h))))+chunk(b'IEND',b'')
with zipfile.ZipFile(Path(c['runtime'])/'mm.o2r') as archive:
 for item,name in [('stone_sword','Kokiri'),('diamond_sword','GreatFairys')]:
  data=archive.read(f'icon_item_static_yar/gItemIcon{name}SwordTex')
  fmt,w,h,size=struct.unpack_from('<4I',data,64)
  assert (fmt,w,h,size)==(1,32,32,4096)
  p=out/f'assets/minecraft/textures/item/{item}.png';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(png(w,h,data[80:]))
p=out/'assets/minecraft/lang/en_us.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps({'item.minecraft.stone_sword':'Kokiri Sword','item.minecraft.diamond_sword':"Great Fairy’s Sword"},indent=2))
(out/'pack.mcmeta').write_text(json.dumps({'pack':{'description':'Local Majora sword appearances — owned assets only','min_format':[97,0],'max_format':[97,1]}},indent=2))
print(out)

# Mask quest tokens use custom item models, leaving ordinary paper unchanged.
mask_icons=['PostmansHat','AllNightMask','BlastMask','StoneMask','GreatFairyMask','DekuMask','KeatonMask','BremenMask','BunnyHood','DonGeroMask','MaskOfScents','GoronMask','RomaniMask','CircusLeaderMask','KafeisMask','CouplesMask','MaskOfTruth','ZoraMask','KamaroMask','GibdoMask','GaroMask','CaptainsHat','GiantsMask','FierceDeityMask']
with zipfile.ZipFile(Path(c['runtime'])/'mm.o2r') as archive:
 for slot,name in enumerate(mask_icons,24):
  data=archive.read(f'icon_item_static_yar/gItemIcon{name}Tex')
  fmt,w,h,size=struct.unpack_from('<4I',data,64)
  assert (fmt,w,h,size)==(1,32,32,4096)
  p=out/f'assets/composite/textures/item/mask_{slot}.png';p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(png(w,h,data[80:]))
  p=out/f'assets/composite/models/item/mask_{slot}.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps({'parent':'minecraft:item/generated','textures':{'layer0':f'composite:item/mask_{slot}'}}))
  p=out/f'assets/composite/items/mask_{slot}.json';p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps({'model':{'type':'minecraft:model','model':f'composite:item/mask_{slot}'}}))
