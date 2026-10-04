#!/usr/bin/env python3
"""Generate the empty dimension datapack locally for world creation."""
import json,zipfile
from pathlib import Path
from common import ROOT, config, client_jar
c=config()
pack=ROOT/'.local/generated/clocktown-native'
jar=client_jar(c)
with zipfile.ZipFile(jar) as z:typ=json.loads(z.read('data/minecraft/dimension_type/overworld.json'))
typ.update(min_y=-64,height=2032,logical_height=2032,ambient_light=0.5)
files={'pack.mcmeta':{'pack':{'description':'Local empty dimension for native Minecraft blocks over Zelda','min_format':[121,0],'max_format':[121,0]}},'data/composite/dimension_type/zelda.json':typ,'data/composite/dimension/zelda.json':{'type':'composite:zelda','generator':{'type':'minecraft:flat','settings':{'biome':'minecraft:plains','lakes':False,'features':False,'layers':[{'block':'minecraft:air','height':1}],'structure_overrides':[]}}}}
for name,data in files.items():
 p=pack/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(data,indent=2)+'\n')
print(pack)
