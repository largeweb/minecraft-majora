#!/usr/bin/env python3
"""ABI and legacy block-shape regression checks; not an end-to-end campaign test."""
import sys,subprocess,tempfile,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from common import *
c=config();cp=str(ROOT/'fabric/build/classes')+os.pathsep+classpath(c)
out=ROOT/'tests/build';out.mkdir(parents=True,exist_ok=True)
run(['g++','-std=c++20','-O2',ROOT/'tests/ipc.cpp','-o',out/'ipc'])
run([java(c,'javac'),'-proc:none','-cp',cp,'-d',out,ROOT/'tests/SharedTest.java',ROOT/'tests/CollisionTest.java'])
with tempfile.TemporaryDirectory(prefix='clocktown-ipc-') as d:
    p=Path(d)/'memory';proc=subprocess.Popen([str(out/'ipc'),str(p)])
    try:
        for _ in range(100):
            if p.exists() and p.stat().st_size==2048:break
            time.sleep(.01)
        run([java(c),'-cp',str(out)+os.pathsep+cp,'SharedTest',p])
        assert proc.wait(timeout=8)==0
    finally:
        if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
run([java(c),'-cp',str(out)+os.pathsep+cp,'local.composite.CollisionTest'])
run([sys.executable,ROOT/'tests/test_launcher.py'])
