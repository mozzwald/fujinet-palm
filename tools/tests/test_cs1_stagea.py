import importlib.util,json,struct,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
def load(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
c=load('cs1stagea',ROOT/'tools/cs1_stagea_collect.py')
class StageA(unittest.TestCase):
 def record(self,**changes):
  r=dict.fromkeys(c.FIELDS,0);r.update(magic=0x564d4131,version=1,module=1,generation=7,cs0=0x28000000,cs_size=0x1000000,cs1=0x29000000,access_ns=330,pc=0x1030,code_base=0x1000,code_size=256,ram_heap_member=1,current_db=2,code_crc=0x1234,id=0x464e,abi=0x100,capabilities=1,status=1,final_ack=4,expected_ack=4,accepted_iterations=4,polls=4,rom_word0=0xfeed,rom_word1=0xbeef);r.update(changes)
  b=struct.pack('>32I',*(r[k] for k in c.FIELDS));return b[:124]+struct.pack('>I',zlib.crc32(b[:124]))
 def test_receipt(self):
  static=dict(text_bytes=256,code_crc32='1234',pc_marker_offset=48)
  self.assertTrue(c.decode_record(self.record(),static)['run_pass'])
  for values in (dict(error=1),dict(final_ack=3),dict(cs1=0x28040000),dict(access_ns=360),dict(ram_heap_member=0),dict(code_crc=0x9999),dict(capabilities=3)):
   self.assertFalse(c.decode_record(self.record(**values),static)['run_pass'])
 def test_crc(self):
  b=bytearray(self.record());b[80]^=1
  with self.assertRaises(ValueError):c.decode_record(b,{})
 def test_actual_build(self):
  v=load('verify_stagea',ROOT/'tools/verify-cs1-stagea.py');p=ROOT/'palm/apps/cs1stagea/build';abi=ROOT.parent/'fujinet-nio/bridges/rp2350-springboard/include/visor_mailbox.h'
  r=v.verify((p/'stagea.prc').read_bytes(),(p/'stagea.text').read_bytes(),(p/'stagea.nm').read_text(),abi.read_bytes());self.assertEqual(r['peripheral_access'],'verified MOVE.W only')

class Service(unittest.TestCase):
 def test_identity_and_explicit_arm(self):
  service=load('cs1service',ROOT/'tools/cs1_stagea_service.py')
  class RP:
   def __init__(self,bad=False):self.sent=[];self.bad=bad;self.active=False
   def send(self,x):self.sent.append(x);self.active=self.active or x=='b'
   def read(self,t):
    if self.sent[-1]=='q':return 'wrong' if self.bad else 'CS1_STAGE_A abi=0100 words=256 crc=%08x clock_khz=150000 starts_disarmed=1 marker=none build=0000000000000000000000000000000000000000000000000000000000000000'%zlib.crc32(b'x'*512)
    return 'active=%d stopped=%d arm_pending=0 run=%d accepted=0 capture_fault=0 status=0001 ack=0000 counters_final=0'%(self.active,not self.active,self.active)
  rp=RP();self.assertEqual(service.inspect(rp,b'x'*512,build_id='0'*64)['active'],0);self.assertEqual(rp.sent,['q','s'])
  rp=RP();self.assertEqual(service.inspect(rp,b'x'*512,True,build_id='0'*64)['active'],1);self.assertEqual(rp.sent,['q','s','b','s'])
  rp=RP(True)
  with self.assertRaises(c.p.CollectionError):service.inspect(rp,b'x'*512,True,build_id='0'*64)
  self.assertEqual(rp.sent,['q'])
 def test_firmware_provenance(self):
  v=load('cs1fw',ROOT/'tools/verify-cs1-stagea-firmware.py');build=ROOT.parent/'handspring-visor-plan/evidence/ram-pivot/2026-10-26-p4a-001';base=ROOT.parent/'handspring-visor-plan/evidence/ram-pivot/2026-10-26-p2-004-autoarm';reference=build/'cs1-stage-a.elf'
  args=[(build/('cs1-stage-a.'+ext)).read_bytes() for ext in ('elf','bin','uf2')]+[(base/'ramsetup-autoarm.rom').read_bytes(),reference.read_bytes(),(base/'ramsetup-autoarm.uf2').read_bytes()]
  self.assertTrue(v.verify(*args)['tested_arm_body_equal'])
  for index in (1,2,3,4):
   changed=list(args);b=bytearray(changed[index]);offset=400
   if index==4:
    e=v.m.ELF(bytes(b));value,n,kind,section=e.symbols['sb_async_reply'];sec=e.sections[section];offset=sec[4]+(value&~1)-sec[3]
   b[offset]^=1;changed[index]=bytes(b)
   with self.assertRaises((ValueError,KeyError,struct.error)):v.verify(*changed)

if __name__=='__main__':unittest.main()
