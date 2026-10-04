from pathlib import Path
import json, os, subprocess, hashlib
ROOT = Path(__file__).resolve().parents[1]
VERSIONS = json.loads((ROOT / 'versions.json').read_text())
def defaults():
    return dict(prism_dir=str(ROOT/'.local/prism'), instance='clocktown-composite',
                runtime=str(ROOT/'.local/runtime'), source=str(ROOT/'.local/2ship'),
                java_home='/usr/lib/jvm/java-25-openjdk',
                shm='/dev/shm/clocktown-'+str(os.getuid())+'-'+hashlib.sha256(str(ROOT).encode()).hexdigest()[:8],
                prism_command='prismlauncher', start='auto')
def config():
    p=Path(os.environ.get('COMPOSITE_CONFIG', ROOT/'local.json'))
    if not p.is_file(): raise RuntimeError('Run ./composite init first (local.json is private and gitignored).')
    c=defaults(); c.update(json.loads(p.read_text()))
    for k in ('prism_dir','runtime','source','java_home'):
        c[k]=str(Path(c[k]).expanduser().resolve())
    if not c['instance'] or any(x in c['instance'] for x in ('/','\\','..')):
        raise RuntimeError('instance must be a simple Prism folder name')
    if not c['shm'].startswith('/dev/shm/') or any(x.isspace() for x in c['shm']):
        raise RuntimeError('shm must be an absolute /dev/shm path without spaces')
    return c
def instance(c): return Path(c['prism_dir'])/'instances'/c['instance']
def minecraft(c): return instance(c)/'minecraft'
def client_jar(c):
    p=Path(c['prism_dir'])/'libraries/com/mojang/minecraft'/VERSIONS['minecraft']/('minecraft-'+VERSIONS['minecraft']+'-client.jar')
    if not p.is_file(): raise RuntimeError('Minecraft libraries missing. Launch the pinned instance in Prism with your account first.')
    return p
def classpath(c):
    client=client_jar(c)
    return os.pathsep.join([str(client)]+[str(p) for p in sorted((Path(c['prism_dir'])/'libraries').rglob('*.jar')) if p!=client])
def java(c, tool='java'): return str(Path(c['java_home'])/'bin'/tool)
def run(args, **kwargs):
    subprocess.run([str(x) for x in args],check=True,**kwargs)
def atomic_copy(src,dst):
    import shutil
    dst=Path(dst); dst.parent.mkdir(parents=True,exist_ok=True)
    tmp=dst.with_name(dst.name+'.new'); shutil.copy2(src,tmp);tmp.replace(dst)
