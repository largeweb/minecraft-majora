#!/usr/bin/env python3
"""Local setup/build/launcher. Never downloads ROMs or handles account credentials."""
import argparse, fcntl, json, os, signal, shutil, struct, subprocess, sys, time
from pathlib import Path
from common import *

def init():
    path=Path(os.environ.get('COMPOSITE_CONFIG',ROOT/'local.json'))
    if path.exists(): raise RuntimeError('Configuration already exists; edit it explicitly instead of overwriting it.')
    c=defaults();path.write_text(json.dumps(c,indent=2)+'\n');path.chmod(0o600)
    scaffold(c)
    print('Created private local.json and an isolated Prism instance. Next: ./composite prism')

def scaffold(c):
    folder=instance(c);folder.mkdir(parents=True,exist_ok=True)
    pack=folder/'mmc-pack.json'
    if pack.exists(): raise RuntimeError('Instance already exists; refusing to replace its settings.')
    pack.write_text(json.dumps({'formatVersion':1,'components':[
        {'uid':'net.minecraft','version':VERSIONS['minecraft'],'important':True},
        {'uid':'net.fabricmc.fabric-loader','version':VERSIONS['fabric_loader']} ]},indent=2)+'\n')
    # QSettings syntax; quoted Java path allows spaces. No account fields are copied.
    (folder/'instance.cfg').write_text('[General]\nInstanceType=OneSix\nname=Clock Town Composite\n'
        'OverrideJavaLocation=true\nJavaPath='+json.dumps(java(c))+'\n'
        'OverrideJavaArgs=true\nJvmArgs='+json.dumps('-Dcomposite.shm='+c['shm']+' -Dcomposite.passthrough=true')+'\n'
        'OverrideMemory=true\nMinMemAlloc=512\nMaxMemAlloc=4096\n')
    minecraft(c).mkdir(parents=True,exist_ok=True)

def fetch(c):
    source=Path(c['source'])
    if source.exists(): raise RuntimeError('Source directory already exists. Use a fresh private path for fetch-source.')
    source.parent.mkdir(parents=True,exist_ok=True)
    run(['git','clone',VERSIONS['2ship_repository'],source])
    run(['git','-C',source,'checkout','--detach',VERSIONS['2ship_commit']])
    run(['git','-C',source,'submodule','update','--init','--recursive'])
    print('Pinned official source fetched. No game data fetched.')

def apply_patch(directory,patch):
    if subprocess.run(['git','-C',str(directory),'apply','--reverse','--check',str(patch)],capture_output=True).returncode==0:
        return
    run(['git','-C',directory,'apply','--check',patch])
    run(['git','-C',directory,'apply',patch])

def build(c,jobs,offline=False):
    source=Path(c['source']);b=source/'build-composite'
    for path,commit in [(source,VERSIONS['2ship_commit']),(source/'libultraship',VERSIONS['libultraship_commit'])]:
        actual=subprocess.check_output(['git','-C',str(path),'rev-parse','HEAD'],text=True).strip()
        if actual!=commit: raise RuntimeError('Upstream revision does not match versions.json: '+str(path))
    client_jar(c)
    apply_patch(source,ROOT/'patches/2ship.patch')
    apply_patch(source/'libultraship',ROOT/'patches/libultraship.patch')
    hook=source/'mm/2s2h/Enhancements/Composite';hook.mkdir(parents=True,exist_ok=True)
    for p in (ROOT/'ship').iterdir():
        if p.is_file(): shutil.copy2(p,hook/p.name)
    run([java(c),ROOT/'tools/ExtractDirt.java',client_jar(c),hook/'DirtTexture.h'])
    args=['cmake','-S',source,'-B',b,'-G','Ninja','-DCMAKE_BUILD_TYPE=Release']
    if offline: args.append('-DFETCHCONTENT_FULLY_DISCONNECTED=ON')
    run(args)
    run(['cmake','--build',b,'--target','2ship','Generate2ShipOtr','--parallel',str(jobs)])
    runtime=Path(c['runtime']);runtime.mkdir(parents=True,exist_ok=True)
    atomic_copy(b/'mm/2s2h.elf',runtime/'2ship-passthrough.elf')
    atomic_copy(b/'mm/2ship.o2r',runtime/'2ship.o2r')
    shutil.copytree(b/'mm/assets',runtime/'assets',dirs_exist_ok=True)
    run([sys.executable,ROOT/'fabric/build.py'])
    print('Native and Fabric builds ready. Supply your own ROM and run ./composite extract.')

def assets(c):
    if not (Path(c['runtime'])/'mm.o2r').is_file(): raise RuntimeError('Missing locally extracted mm.o2r; run ./composite extract first.')
    run([sys.executable,ROOT/'tools/build-sword-pack.py'])
    run([sys.executable,ROOT/'tools/build-play-dimension.py'])
    print('Generated private resource pack and datapack in .local/generated/.')

def install(c):
    if any(kind=='Minecraft' for kind in sessions(c).values()): raise RuntimeError('Save and close this Minecraft instance before installing.')
    mc=minecraft(c)
    if not (instance(c)/'mmc-pack.json').is_file(): raise RuntimeError('Instance missing; run init or check local.json.')
    atomic_copy(ROOT/'fabric/build/clocktown-composite.jar',mc/'mods/clocktown-composite.jar')
    shutil.copytree(ROOT/'.local/generated/sword-pack',mc/'resourcepacks/Local Zelda Swords',dirs_exist_ok=True)
    world=mc/'saves/Composite_World'
    if (world/'level.dat').is_file():
        shutil.copytree(ROOT/'.local/generated/clocktown-native',world/'datapacks/clocktown-native',dirs_exist_ok=True)
    print('Installed mod/resource pack. Enable Local Zelda Swords in Minecraft. See README for first world creation.')

def native_env(c,composite=True):
    env=os.environ.copy()
    for k in ('COMPOSITE_SHM','COMPOSITE_FRAME','COMPOSITE_BOOT','COMPOSITE_STORY','COMPOSITE_START','COMPOSITE_TEST_INPUT','COMPOSITE_TEST_SONG','COMPOSITE_PLAYTEST'):
        env.pop(k,None)
    env.update(SHIP_HOME=c['runtime'],SDL_VIDEODRIVER='x11')
    if composite:
        env.update(COMPOSITE_SHM=c['shm'],COMPOSITE_FRAME=c['shm']+'.rgba',COMPOSITE_BOOT='1',COMPOSITE_STORY='1')
        if c.get('start') in ('lesson','field'): env['COMPOSITE_START']=c['start']
        elif c.get('start')=='auto' and not (Path(c['runtime'])/'composite-checkpoint.json').exists():env['COMPOSITE_START']='lesson'
    return env

def sessions(c):
    found={};binary=(Path(c['runtime'])/'2ship-passthrough.elf').resolve();mc=minecraft(c).resolve()
    for proc in Path('/proc').glob('[0-9]*'):
        try:
            args=(proc/'cmdline').read_bytes().split(b'\0');exe=(proc/'exe').resolve();cwd=(proc/'cwd').resolve()
            if exe==binary and cwd==Path(c['runtime']).resolve(): found[int(proc.name)]='Zelda'
            elif exe.name=='java' and ('-Dcomposite.shm='+c['shm']).encode() in args and cwd==mc:
                found[int(proc.name)]='Minecraft'
        except (OSError,ValueError): pass
    return found

def alive(pid):
    try:return Path('/proc',str(pid),'stat').read_text().rsplit(')',1)[1].split()[0] not in ('Z','X')
    except OSError:return False

def stop(c):
    targets=sessions(c)
    for pid,kind in targets.items():
        if sessions(c).get(pid)!=kind:continue
        try:os.kill(pid,signal.SIGTERM)
        except ProcessLookupError:pass
        print('Closing',kind,pid,flush=True)
    deadline=time.monotonic()+45
    while any(alive(pid) for pid in targets):
        if time.monotonic()>deadline: raise RuntimeError('A game is still closing. Save and Quit or close its window; nothing was force-killed. Try again afterward.')
        time.sleep(.25)
    print('This shared session is stopped. Other apps were left open.')

def doctor(c):
    ok=True
    def check(label,condition):
        nonlocal ok
        print(('OK   ' if condition else 'MISS ')+label);ok &= bool(condition)
    check('Linux /dev/shm transport',sys.platform=='linux' and Path('/dev/shm').is_dir())
    for name in ('git','cmake','ninja',c['prism_command']):check(name,shutil.which(name))
    check('configured Java executable',Path(java(c)).is_file())
    try:
        pack=json.loads((instance(c)/'mmc-pack.json').read_text())
        versions={x['uid']:x.get('version') for x in pack['components']}
        pinned=versions.get('net.minecraft')==VERSIONS['minecraft'] and versions.get('net.fabricmc.fabric-loader')==VERSIONS['fabric_loader']
    except (OSError,ValueError,KeyError):pinned=False
    check('Prism instance Minecraft/Fabric version pins',pinned)
    try:client_jar(c);present=True
    except RuntimeError:present=False
    check('Minecraft '+VERSIONS['minecraft']+' local client jar',present)
    for name in ('2ship-passthrough.elf','2ship.o2r','mm.o2r'):
        check('runtime/'+name,(Path(c['runtime'])/name).is_file())
    check('Fabric mod installed',(minecraft(c)/'mods/clocktown-composite.jar').is_file())
    check('Composite_World created',(minecraft(c)/'saves/Composite_World/level.dat').is_file())
    check('composite dimension installed',(minecraft(c)/'saves/Composite_World/datapacks/clocktown-native/pack.mcmeta').is_file())
    print('Hyprland layout helper:', 'available' if shutil.which('hyprctl') else 'unavailable; arrange windows manually')
    return ok

def configure_window(c):
    p=Path(c['runtime'])/'2ship2harkinian.json'
    d=json.loads(p.read_text()) if p.exists() else {}
    w=d.setdefault('Window',{});w.setdefault('Fullscreen',{})['Enabled']=False;w.update(Width=1280,Height=720)
    w['Backend']={'Id':2,'Name':'OpenGL'}
    d.setdefault('CVars',{}).setdefault('gOpenWindows',{})['Console']=0
    p.write_text(json.dumps(d,indent=2)+'\n')

def start(c):
    live=sessions(c)
    if live:
        print('Already running:',', '.join(live.values()),'— use restart for a clean pair.');return
    if not doctor(c):raise RuntimeError('Finish the missing setup steps above before starting.')
    configure_window(c)
    logs=ROOT/'.local/logs';logs.mkdir(parents=True,exist_ok=True)
    with (logs/'world.log').open('w') as log:
        world=subprocess.Popen([str(Path(c['runtime'])/'2ship-passthrough.elf')],cwd=c['runtime'],env=native_env(c),stdout=log,stderr=log,start_new_session=True)
    time.sleep(1)
    if world.poll() is not None:raise RuntimeError('Zelda exited; see .local/logs/world.log')
    with (logs/'player.log').open('w') as log:
        player=subprocess.Popen([c['prism_command'],'--dir',c['prism_dir'],'--launch',c['instance']],stdout=log,stderr=log,start_new_session=True)
    time.sleep(1)
    if player.poll() not in (None,0):raise RuntimeError('Prism exited with an error; see .local/logs/player.log')
    if os.environ.get('HYPRLAND_INSTANCE_SIGNATURE') and shutil.which('hyprctl'):
        with (logs/'layout.log').open('w') as log:
            subprocess.Popen([sys.executable,str(ROOT/'tools/arrange.py'),'--wait'],stdout=log,stderr=log,start_new_session=True)
    print('Started. Play in Minecraft x Zelda. Choose Create Backup and Load if Minecraft prompts.')

def mask_session(c):
    if sessions(c):raise RuntimeError('Stop this shared session before creating an all-masks copy.')
    old=Path(c['runtime']);checkpoint=old/'composite-checkpoint.json'
    if not checkpoint.is_file():raise RuntimeError('Finish Song of Healing and reach Clock Town to create a checkpoint first.')
    save=json.loads(checkpoint.read_text())
    if save.get('format')!=1:raise RuntimeError('Unsupported checkpoint format')
    items=save['save']['saveInfo']['inventory']['items']
    if len(items)!=48:raise RuntimeError('Unexpected native inventory layout')
    # Native IDs in inventory-slot order, from the pinned z64item.h; no game assets.
    masks=[0x3e,0x38,0x47,0x45,0x40,0x32,0x3a,0x46,0x39,0x42,0x48,0x33,
           0x3c,0x3d,0x37,0x3f,0x36,0x34,0x43,0x41,0x3b,0x44,0x49,0x35]
    items[24:48]=masks
    target=ROOT/'.local/sessions'/('all-masks-'+str(time.time_ns()));target.mkdir(parents=True)
    for name in ('2ship-passthrough.elf','2ship.o2r','mm.o2r','assets'):
        src=old/name
        if not src.exists():raise RuntimeError('Missing runtime file: '+name)
        (target/name).symlink_to(src.resolve())
    config_file=old/'2ship2harkinian.json'
    if config_file.exists():shutil.copy2(config_file,target/config_file.name)
    (target/'composite-checkpoint.json').write_text(json.dumps(save,indent=2)+'\n')
    path=Path(os.environ.get('COMPOSITE_CONFIG',ROOT/'local.json'))
    data=json.loads(path.read_text());data.setdefault('base_runtime',str(old));data['runtime']=str(target);data['start']='town'
    backup=path.with_name(path.name+'.bak');backup.write_text(path.read_text());backup.chmod(0o600)
    path.write_text(json.dumps(data,indent=2)+'\n');path.chmod(0o600)
    print('All 24 masks unlocked in a separate session copy. Original checkpoint unchanged.')
    print('Run ./composite start, then use creative inventory to get mask items.')
    print('To restore normal ownership, stop first and set runtime back to base_runtime in local.json.')

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--config',help='private JSON configuration (default: local.json)')
    sub=p.add_subparsers(dest='cmd',required=True)
    for n in ('init','prism','fetch-source','build-fabric','extract','assets','install','doctor','start','stop','restart','status','arrange','test','audit','session-masks'):
        sub.add_parser(n)
    b=sub.add_parser('build');b.add_argument('--jobs',type=int,default=min(8,os.cpu_count() or 2));b.add_argument('--offline',action='store_true')
    a=p.parse_args()
    if a.config:os.environ['COMPOSITE_CONFIG']=str(Path(a.config).resolve())
    if a.cmd=='init':init();return
    if a.cmd=='audit':run([sys.executable,ROOT/'tools/audit.py']);return
    c=config()
    if a.cmd=='prism':run([c['prism_command'],'--dir',c['prism_dir']]);return
    if a.cmd=='fetch-source':fetch(c)
    elif a.cmd=='build':build(c,max(1,a.jobs),a.offline)
    elif a.cmd=='build-fabric':run([sys.executable,ROOT/'fabric/build.py'])
    elif a.cmd=='extract':
        if sessions(c):raise RuntimeError('Stop this session before running extraction.')
        run([Path(c['runtime'])/'2ship-passthrough.elf'],cwd=c['runtime'],env=native_env(c,False))
    elif a.cmd=='assets':assets(c)
    elif a.cmd=='install':install(c)
    elif a.cmd=='doctor':sys.exit(0 if doctor(c) else 1)
    elif a.cmd=='status':
        found=sessions(c)
        for pid,kind in found.items():print(kind+':',pid)
        if not found:print('Stopped')
    elif a.cmd=='arrange':run([sys.executable,ROOT/'tools/arrange.py'])
    elif a.cmd=='test':run([sys.executable,ROOT/'tests/run.py'])
    elif a.cmd in ('start','stop','restart','session-masks'):
        state=ROOT/'.local';state.mkdir(exist_ok=True)
        with (state/'launcher.lock').open('w') as lock:
            try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
            except BlockingIOError:raise RuntimeError('Another launcher operation is already running.')
            if a.cmd=='session-masks':mask_session(c)
            if a.cmd in ('stop','restart'):stop(c)
            if a.cmd in ('start','restart'):start(c)

if __name__=='__main__':
    try:main()
    except (RuntimeError,OSError,subprocess.CalledProcessError,ValueError) as e:
        print('Error:',e,file=sys.stderr);sys.exit(1)
