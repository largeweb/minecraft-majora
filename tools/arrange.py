#!/usr/bin/env python3
"""Optional layout helper for the tested Omarchy Lua Hyprland dispatcher."""
import json, os, subprocess, sys, time
from common import config
from manage import sessions

def clients():return json.loads(subprocess.check_output(['hyprctl','clients','-j']))
def dispatch(code):subprocess.run(['hyprctl','dispatch',code],check=True,stdout=subprocess.DEVNULL)
def main():
    c=config();deadline=time.monotonic()+(90 if '--wait' in sys.argv else 0)
    while True:
        owned=sessions(c);cs=clients()
        try:
            world=next(w for w in cs if owned.get(w['pid'])=='Zelda' and w['title']!='Console')
            player=next(w for w in cs if owned.get(w['pid'])=='Minecraft')
            break
        except StopIteration:
            if time.monotonic()>=deadline:raise RuntimeError('Launch both games first, then run ./composite arrange.')
            time.sleep(1)
    for w in (world,player):
        dispatch('hl.dsp.focus({window='+json.dumps('address:'+w['address'])+'})')
        time.sleep(.2)
        actual=json.loads(subprocess.check_output(['hyprctl','activewindow','-j']))
        if actual.get('address')!=w['address']:
            raise RuntimeError('Cannot focus game. Unlock desktop and retry; target was not changed.')
        if not actual['floating']:dispatch('hl.dsp.window.float({action="set"})')
        # Keep Minecraft fullscreen if the player chose it. Source starts windowed.
        if not actual.get('fullscreen'):
            dispatch('hl.dsp.window.resize({x=1280,y=720,relative=false})')
            dispatch('hl.dsp.window.move({x=300,y=200,relative=false})')
        if w is world:dispatch('hl.dsp.window.move({workspace="name:Zelda-renderer",follow=false})')
    print('Minecraft is the play window. Zelda frame source is on workspace Zelda-renderer.')
if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,subprocess.CalledProcessError) as e:print(e,file=sys.stderr);sys.exit(1)
