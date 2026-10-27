import importlib.util,json,struct,sys,unittest,zlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'));import timing_collect as t
class Timing(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.static=json.loads(Path('/tmp/visor-timing-static.json').read_text())
 def record(self,event=1):
  d={k:0 for k in t.FIELDS};d.update(magic=0x46544d33,version=1,event=event,flags=14,stage=7,base=0x28000000,original=330,applied=330 if event==1 else 360,requested=0 if event==1 else 360,restored=330,word0=0xfeed,word1=0xbeef,longword=0xfeedbeef,ram_heap_member=1,current_db=100,code_base=0x10000,code_size=self.static['text_bytes'],pc=0x10000+self.static['pc_marker_offset'],code_crc=int(self.static['code_crc32'],16));return d
 def encode(self,r):
  b=struct.pack('>48I',*(r[k] for k in t.FIELDS));return b[:188]+struct.pack('>I',zlib.crc32(b[:188]))
 def test_pass(self):
  for mode in (1,2):self.assertTrue(t.decode_record(self.encode(self.record(mode)),self.static)['run_pass'])
 def test_restore_data_identity_proof(self):
  for k,v in [('restored',360),('error',1),('flags',15),('longword',0xfeedfeed),('guard_error',1),('heap_flags',1),('ram_heap_member',0),('pc',0x28000000),('code_crc',0)]:
   r=self.record(2);r[k]=v;self.assertFalse(t.decode_record(self.encode(r),self.static)['run_pass'],k)
 def test_codec_corruption(self):
  b=bytearray(self.encode(self.record()));b[30]^=1
  with self.assertRaises(ValueError):t.decode_record(bytes(b),self.static)
class Assembly(unittest.TestCase):
 def test_exact_loads(self):
  s=importlib.util.spec_from_file_location('verifytiming',ROOT/'tools/verify-timing-app.py');m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
  p=ROOT/'palm/apps/timing/build';prc=(p/'timing.prc').read_bytes();text=(p/'timing.text').read_bytes();nm=(p/'timing.nm').read_text();r=m.verify(prc,text,nm);self.assertGreater(r['text_bytes'],100)
  b=bytearray(text);b[r['loads_offset']+24]^=1
  with self.assertRaises(ValueError):m.verify(prc,bytes(b),nm)
if __name__=='__main__':unittest.main()
