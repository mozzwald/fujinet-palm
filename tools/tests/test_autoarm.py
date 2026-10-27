import importlib.util,json,os,sys,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import autoarm_status
import prism_collect as p
class Status(unittest.TestCase):
 def test_only_read_commands(self):
  class Fake:
   sent=[]
   def send(self,c):self.sent.append(c)
   def read(self,n):return f'CONTINUOUS version=3 response=cpu-rom marker=preload continuous=1 words=256 crc={zlib.crc32(bytes(512)):08x} clock_khz=150000 boot_autoarm=1'
   def status(self):self.sent.append('s');return dict(rom_ready=1,ready=1,active=1)
  rp=Fake();self.assertEqual(autoarm_status.inspect(rp,bytes(512))['service'],'ACTIVE');self.assertEqual(rp.sent,['q','s'])
 def test_manual_firmware_rejected_without_arm(self):
  class Fake:
   sent=[]
   def send(self,c):self.sent.append(c)
   def read(self,n):return 'CONTINUOUS version=3'
  rp=Fake()
  with self.assertRaises(p.CollectionError):autoarm_status.inspect(rp,bytes(512))
  self.assertEqual(rp.sent,['q'])
class Provenance(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.path=Path(os.environ.get('AUTOARM_FIRMWARE_DIR','/tmp/visor-ramsetup-autoarm-firmware'))
  if not (cls.path/'springboard_rom_boot_autoarm.elf').exists():raise unittest.SkipTest('build autoarm fixture first')
  spec=importlib.util.spec_from_file_location('armverify',ROOT/'tools/verify-autoarm-firmware.py');cls.m=importlib.util.module_from_spec(spec);spec.loader.exec_module(cls.m)
  baseline=ROOT.parent/'handspring-visor-plan/evidence/phase-3/2026-10-25-p03-011-persistent-reply'
  cls.args=[(cls.path/f'springboard_rom_boot_autoarm.{x}').read_bytes() for x in ('elf','bin','uf2')]+[(baseline/f'persistent-reply.{x}').read_bytes() for x in ('elf','uf2')]+[(ROOT.parent/'handspring-visor-plan/evidence/ram-pivot/2026-10-26-p2-002/ramsetup-p2.rom').read_bytes(),json.loads(Path('/tmp/visor-autoarm-startup-tests.json').read_text())]
 def test_provenance_and_truthful_boot_policy(self):
  r=self.m.verify(*self.args);self.assertFalse(r['starts_disarmed']);self.assertTrue(r['cold_initial_state_disarmed']);self.assertTrue(r['critical_addresses_equal']);self.assertTrue(r['autoarm_once_per_boot'])
 def test_wrong_startup_binding(self):
  a=list(self.args);a[-1]=dict(a[-1],elf_sha256='wrong')
  with self.assertRaisesRegex(ValueError,'startup proof'):self.m.verify(*a)
 def test_failed_startup(self):
  a=list(self.args);a[-1]=dict(a[-1],result='FAIL')
  with self.assertRaises(ValueError):self.m.verify(*a)
if __name__=='__main__':unittest.main()
