"""Exercise launcher isolation, duplicate handling and safe shutdown without games."""
import importlib.util,sys,tempfile,unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import manage
class LauncherTest(unittest.TestCase):
    def test_existing_session_does_not_launch(self):
        with patch.object(manage,'sessions',return_value={123:'Minecraft'}),patch.object(manage.subprocess,'Popen') as launch:
            manage.start({});launch.assert_not_called()
    def test_pid_reuse_is_not_signalled(self):
        with patch.object(manage,'sessions',side_effect=[{123:'Minecraft'},{123:'Zelda'}]),patch.object(manage,'alive',return_value=False),patch.object(manage.os,'kill') as kill:
            manage.stop({});kill.assert_not_called()
    def test_only_recognized_games_are_signalled(self):
        with patch.object(manage,'sessions',return_value={123:'Minecraft'}),patch.object(manage,'alive',return_value=False),patch.object(manage.os,'kill') as kill:
            manage.stop({});kill.assert_called_once_with(123,manage.signal.SIGTERM)
    def test_configuration_is_isolated(self):
        c=manage.defaults()
        self.assertTrue(str(manage.ROOT/'.local') in c['runtime'])
        self.assertTrue(c['shm'].startswith('/dev/shm/'))
    def test_scaffold_refuses_existing_instance(self):
        with tempfile.TemporaryDirectory() as tmp:
            c=manage.defaults();c['prism_dir']=tmp
            manage.scaffold(c)
            with self.assertRaises(RuntimeError):manage.scaffold(c)
    def test_mask_session_preserves_original_checkpoint(self):
        import json,os
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);runtime=root/'runtime';runtime.mkdir()
            initial={'format':1,'save':{'saveInfo':{'inventory':{'items':[255]*48}}}}
            checkpoint=runtime/'composite-checkpoint.json';checkpoint.write_text(json.dumps(initial))
            for name in ('2ship-passthrough.elf','2ship.o2r','mm.o2r','assets'):(runtime/name).touch()
            cfg=root/'local.json';cfg.write_text(json.dumps({'runtime':str(runtime)}))
            with patch.object(manage,'ROOT',root),patch.object(manage,'sessions',return_value={}),patch.dict(os.environ,{'COMPOSITE_CONFIG':str(cfg)}):
                manage.mask_session({'runtime':str(runtime)})
            changed=json.loads(cfg.read_text());copy=json.loads((Path(changed['runtime'])/'composite-checkpoint.json').read_text())
            self.assertEqual(json.loads(checkpoint.read_text()),initial)
            self.assertEqual(set(copy['save']['saveInfo']['inventory']['items'][24:]),set(range(0x32,0x4a)))
            self.assertEqual(changed['base_runtime'],str(runtime))
if __name__=='__main__':unittest.main()
