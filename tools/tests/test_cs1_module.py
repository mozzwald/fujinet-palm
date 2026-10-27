import importlib.util,json,struct,unittest,zlib,os
from pathlib import Path
import prism_collect as p
import cs1_module_collect as c
ROOT=Path(__file__).resolve().parents[2]
def load(name,file):
 s=importlib.util.spec_from_file_location(name,ROOT/'tools'/file);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
class Module(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.static=json.loads(Path(os.environ.get('CS1_MODULE_STATIC','/tmp/p4m-b-static.json')).read_text());cls.build=ROOT/'palm/apps/cs1setup/build'
 def test_full_code_and_rom(self):
  v=load('modulecode','verify-cs1setup-code.py');b=self.build;abi=ROOT.parent/'fujinet-nio/bridges/rp2350-springboard/include/visor_mailbox.h'
  r=v.verify((b/'setup.prc').read_bytes(),(b/'setup.text').read_bytes(),(b/'setup.nm').read_text(),(b/'setup.dis').read_text(),(b/'setup.sections').read_text(),abi.read_bytes());self.assertEqual(r['setup_proof']['globals_bytes'],0)
  rom=load('modulerom','verify-cs1setup-rom.py');self.assertEqual(rom.verify((b/'setup.rom').read_bytes(),(b/'setup.prc').read_bytes())['expected_runtime_creator'],self.static['runtime_creator'])
  bad=bytearray((b/'setup.prc').read_bytes());bad[60:64]=b'appl'
  with self.assertRaises(ValueError):rom.verify((b/'setup.rom').read_bytes(),bytes(bad))
 def test_install_and_run_join(self):
  s=self.static;setup=dict.fromkeys(c.install.FIELDS,0)
  setup.update(magic=0x46525032,format=1,event=2,generation=1,module=1,runtime_db=42,source_db=21,stage=7,receipt_active=1,ram_heap_checks=1,source_crc=int(s['code_crc32'],16),setup_base=0x1000,setup_size=s['text_bytes'],setup_pc=0x1000+s['setup_proof']['pc_marker_offset'])
  installation=bytearray(p.make_pdb(s['install_log_name'],[struct.pack('>32I',*(setup[k] for k in c.install.FIELDS))],b'DATA'));installation[64:68]=s['runtime_creator'].encode()
  r=dict.fromkeys(c.stage.FIELDS,0);r.update(magic=0x564d4131,version=1,generation=1,module=1,current_db=42,cs0=0x28000000,cs_size=0x1000000,cs1=0x29000000,access_ns=330,code_base=0x5000,code_size=s['text_bytes'],pc=0x5000+s['pc_marker_offset'],code_crc=int(s['code_crc32'],16),ram_heap_member=1,id=0x464e,abi=0x100,capabilities=1,status=1,final_ack=4,expected_ack=4,accepted_iterations=4,polls=4,rom_word0=0xfeed,rom_word1=0xbeef)
  def payload():
   b=struct.pack('>32I',*(r[k] for k in c.stage.FIELDS));b=b[:124]+struct.pack('>I',zlib.crc32(b[:124]));d=bytearray(p.make_pdb(s['log_name'],[b],b'DATA'));d[64:68]=s['runtime_creator'].encode();return bytes(d)
  self.assertTrue(c.decode(bytes(installation),payload(),s)['matching_install_and_runtime'])
  r['current_db']=43;self.assertFalse(c.decode(bytes(installation),payload(),s)['matching_install_and_runtime'])
 def test_ram_clone_and_open_flag(self):
  source=(self.build/'setup.prc').read_bytes();app=bytearray(source);name=self.static['runtime_name'].encode();app[:32]=name+bytes(32-len(name));app[60:68]=b'appl'+self.static['runtime_creator'].encode();app[32:34]=bytes.fromhex('8009')
  self.assertTrue(c.verify_clone(bytes(app),source,self.static)['all_resources_equal'])
  app[32:34]=bytes.fromhex('0009');self.assertTrue(c.verify_clone(bytes(app),source,self.static)['metadata_valid'])
  app[32:34]=bytes.fromhex('800b')
  with self.assertRaises(ValueError):c.verify_clone(bytes(app),source,self.static)
  app[32:34]=bytes.fromhex('8009');app[-1]^=1
  with self.assertRaises(ValueError):c.verify_clone(bytes(app),source,self.static)
 def test_bounded_debug_records(self):
  formats=[('CS1_STAGE_A abi=%04x words=%u crc=%08x clock_khz=%u starts_disarmed=%u marker=none boot_autoarm=%u', (256,32768,0xffffffff,150000,1,1)),('BUILD %s',('b'*64,)),('active=%u stopped=%u arm_pending=%u run=%u accepted=%u capture_fault=%u status=%04x ack=%04x counters_final=%u',(1,1,1,0xffffffff,0xffffffff,8,65535,65535,1))]
  for fmt,args in formats:self.assertLess(len((fmt%args).encode())+1,128)
 def test_read_only_service(self):
  service=load('moduleservice','cs1_module_service.py')
  class RP:
   def __init__(self):self.sent=[]
   def send(self,x):self.sent.append(x)
   def read(self,t):
    if self.sent[-1]=='q':return 'CS1_STAGE_A abi=0100 words=256 crc=%08x clock_khz=150000 starts_disarmed=0 marker=none boot_autoarm=1'%zlib.crc32(b'x'*512)
    if self.sent[-1]=='i':return 'BUILD fixture\n'
    return 'active=0 stopped=0 arm_pending=1 run=1 accepted=0 capture_fault=0 status=0001 ack=0000 counters_final=0'
  rp=RP();self.assertEqual(service.inspect(rp,b'x'*512,'fixture')['arm_pending'],1);self.assertEqual(rp.sent,['q','i','s'])
if __name__=='__main__':unittest.main()
