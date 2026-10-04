#!/usr/bin/env python3
"""Build against the user's installed, pinned Minecraft/Fabric libraries. No downloads."""
import sys, shutil
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from common import *
c=config();out=ROOT/'fabric/build/classes'
if out.exists(): shutil.rmtree(out)
out.mkdir(parents=True)
run([java(c,'javac'),'-proc:none','--release',str(VERSIONS['java']),'-classpath',classpath(c),'-d',out,*sorted((ROOT/'fabric/src/main/java').rglob('*.java'))])
shutil.copytree(ROOT/'fabric/src/main/resources',out,dirs_exist_ok=True)
run([java(c,'jar'),'--create','--file',ROOT/'fabric/build/clocktown-composite.jar','-C',out,'.'])
print('Built fabric/build/clocktown-composite.jar (local output only)')
