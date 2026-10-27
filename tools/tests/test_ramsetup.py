import binascii
import importlib.util
import json
import os
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import prism_collect as p
import ramsetup_results as results
import ramsetup_service as service

def module(name,file):
    spec=importlib.util.spec_from_file_location(name,ROOT/'tools'/file)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
romcheck=module('ramrom','verify-ramsetup-rom.py')
codecheck=module('ramcode','verify-ramsetup-code.py')
prepare=module('ramprepare','prepare-ramsetup-prc.py')
BUILD=ROOT/'palm/apps/ramsetup/build'

def reseal(data):
    b=bytearray(data);crc=binascii.crc_hqx(b[:112],0);crc=binascii.crc_hqx(b[114:],crc)
    b[112:114]=crc.to_bytes(2,'big');return bytes(b)

class ROM(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rom=(BUILD/'ramsetup.rom').read_bytes();cls.prc=(BUILD/'ramsetup.prc').read_bytes()
        cls.manifest=romcheck.verify(cls.rom,cls.prc)
    def test_all_resources_equal(self):
        self.assertEqual(self.manifest['type'],'HsSU');self.assertFalse(self.manifest['hardware_verified'])
    def test_bad_crc(self):
        b=bytearray(self.rom);b[400]^=1
        with self.assertRaisesRegex(ValueError,'CRC'):romcheck.verify(b,self.prc)
    def test_bad_case_even_with_valid_crc(self):
        b=bytearray(self.rom);at=self.manifest['database_offset']+63;b[at]=ord('u')
        with self.assertRaisesRegex(ValueError,'case'):romcheck.verify(reseal(b),self.prc)
    def test_extra_database(self):
        b=bytearray(self.rom);d=int.from_bytes(b[316:320],'big')-0x8000000;b[d+4:d+6]=b'\0\2'
        with self.assertRaisesRegex(ValueError,'one database'):romcheck.verify(reseal(b),self.prc)
    def test_welcome_token(self):
        b=bytearray(self.rom);d=int.from_bytes(b[100:104],'big')-0x8000000;b[d:d+4]=b'HsWT'
        with self.assertRaisesRegex(ValueError,'HsAT'):romcheck.verify(reseal(b),self.prc)
    def test_resource_byte_with_valid_card_crc(self):
        b=bytearray(self.rom);b[self.manifest['resources'][0]['offset']]^=1
        with self.assertRaisesRegex(ValueError,'payload'):romcheck.verify(reseal(b),self.prc)
    def test_reset_flag(self):
        b=bytearray(self.rom);d=self.manifest['database_offset'];b[d+33]|=0x20
        with self.assertRaisesRegex(ValueError,'attrs'):romcheck.verify(reseal(b),self.prc)
    def test_bad_chunk_pointer(self):
        b=bytearray(self.rom);d=self.manifest['database_offset'];b[d+84:d+88]=(0xffffffff).to_bytes(4,'big')
        with self.assertRaisesRegex(ValueError,'pointer'):romcheck.verify(reseal(b),self.prc)
    def test_metadata_writer_is_narrow(self):
        raw=(BUILD/'raw.prc').read_bytes();self.assertEqual(prepare.prepare(raw),self.prc)
        b=bytearray(raw);b[64:68]=b'FNVR'
        with self.assertRaises(ValueError):prepare.prepare(b)
        with self.assertRaises(ValueError):prepare.prepare(self.prc)

class Code(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prc=(BUILD/'ramsetup.prc').read_bytes();cls.text=(BUILD/'ramsetup.text').read_bytes()
        cls.nm=(BUILD/'ramsetup.nm').read_text();cls.dis=(BUILD/'ramsetup.dis').read_text();cls.sec=(BUILD/'ramsetup.sections').read_text()
        cls.static=codecheck.verify(cls.prc,cls.text,cls.nm,cls.dis,cls.sec)
    def test_pc_and_globals(self):
        self.assertEqual(self.static['application_A5_dependencies'],0)
        self.assertGreater(self.static['reachable_application_instructions'],500)
    def test_changed_code_resource(self):
        b=bytearray(self.prc);b[-1]^=1
        # Mutate the known code1 bytes specifically.
        b=bytearray(self.prc);n=int.from_bytes(b[76:78],'big')
        for i in range(n):
            t,k,o=struct.unpack_from('>4sHI',b,78+10*i)
            if (t,k)==(b'code',1):b[o]^=1;break
        with self.assertRaisesRegex(ValueError,'exact linked'):codecheck.verify(b,self.text,self.nm,self.dis,self.sec)
    def test_global_storage(self):
        sec=self.sec.replace('.data         00000000','.data         00000004')
        with self.assertRaisesRegex(ValueError,'globals'):codecheck.verify(self.prc,self.text,self.nm,self.dis,sec)
    def test_reachable_a5(self):
        # Use an address-independent replacement of the first reachable marker op.
        import re
        dis=re.sub(r'(\blea )%pc@\([0-9a-f]+ <fnram_pc>\),%a0',r'\1%a5@,%a0',self.dis)
        with self.assertRaisesRegex(ValueError,'A5-dependent'):codecheck.verify(self.prc,self.text,self.nm,dis,self.sec)

class Records(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.static=Code.static if hasattr(Code,'static') else codecheck.verify((BUILD/'ramsetup.prc').read_bytes(),(BUILD/'ramsetup.text').read_bytes(),(BUILD/'ramsetup.nm').read_text(),(BUILD/'ramsetup.dis').read_text(),(BUILD/'ramsetup.sections').read_text())
    def record(self,event=4,generation=1):
        s=self.static
        r=dict.fromkeys(results.FIELDS,0);r.update(magic=0x46525032,format=1,event=event,generation=generation,module=1,
            runtime_db=88,source_db=44,context=88 if event in (4,5) else 0,stage=7,receipt_active=1,ram_heap_checks=3,
            source_crc=int(s['code_crc32'],16),run_crc=int(s['code_crc32'],16),
            setup_base=0x1000,setup_size=s['text_bytes'],setup_pc=0x1000+s['pc_marker_offset'],
            run_base=0x9000,run_size=s['text_bytes'],run_pc=0x9000+s['pc_marker_offset'],
            loop_start=0x9000+s['loop_start_offset'],loop_end=0x9000+s['loop_end_offset'])
        return r
    def encode(self,r):return struct.pack('>32I',*(r[k] for k in results.FIELDS))
    def test_cross_C_codec(self):
        b=(ROOT/'palm/apps/ramsetup/build-host/codec.golden').read_bytes()
        r=results.decode_record(b,self.static);self.assertEqual(r['generation'],0x12340003)
        self.assertEqual(r['ram_heap_checks'],0x1234001f)
    def test_runtime_and_loop(self):
        r=self.record();self.assertTrue(results.decode_record(self.encode(r),self.static)['runtime_ram_evidence'])
        r['event']=5;r['loop_result']=0x52504c50
        self.assertTrue(results.decode_record(self.encode(r),self.static)['loop_evidence'])
        r['receipt_active']=0
        self.assertTrue(results.decode_record(self.encode(r),self.static)['runtime_ram_evidence'])
    def test_false_proof(self):
        for key,value in [('run_heapflags',1),('run_card',1),('ram_heap_checks',1),('run_pc',0x28000000),('source_crc',0),('run_crc',0),('context',99),('stage',6),('error',1),('loop_end',0x9000)]:
            r=self.record();r[key]=value
            self.assertFalse(results.decode_record(self.encode(r),self.static)['runtime_ram_evidence'],key)
    def test_install_generation_correlation(self):
        a=self.encode(self.record(2));b=self.encode(self.record(4))
        blob=bytearray(p.make_pdb('FN RAM Proof',[a,b],b'DATA'));blob[64:68]=b'FNRA'
        r=results.decode_pdb(blob,self.static);self.assertTrue(r['matching_install_and_runtime'])
        self.assertFalse(r['physical_scope_verified']);self.assertEqual(p.DB_CREATOR,b'FNVC')
        b=self.encode(self.record(4,2));blob=bytearray(p.make_pdb('FN RAM Proof',[a,b],b'DATA'));blob[64:68]=b'FNRA'
        self.assertFalse(results.decode_pdb(blob,self.static)['matching_install_and_runtime'])
    def test_record_rejection(self):
        with self.assertRaises(ValueError):results.decode_record(bytes(127),self.static)
        with self.assertRaises(ValueError):results.decode_record(bytes(128),self.static)

class FakeRP:
    def __init__(self,states,identity):self.states=iter(states);self.identity=identity;self.sent=[]
    def status(self):return next(self.states)
    def send(self,c):self.sent.append(c);assert c!='x'
    def read(self,seconds):return self.identity

class Service(unittest.TestCase):
    def states(self):
        b=dict(rom_ready=1,ready=1,stopped=1,arm_pending=0,active=0,run=0)
        return [b,dict(b,stopped=0,run=1),dict(b,stopped=0,active=1,run=1)]
    def test_guarded_arm_without_stop(self):
        import zlib
        rom=b'x'*512;identity=f'CONTINUOUS version=3 response=cpu-rom marker=preload continuous=1 words=256 crc={zlib.crc32(rom):08x} clock_khz=150000'
        rp=FakeRP(self.states(),identity)
        with tempfile.TemporaryDirectory() as d:
            self.assertTrue(service.prime(rp,rom,Path(d),announce=lambda *a,**k:None)['active'])
        self.assertEqual(rp.sent,['R','q','b'])
    def test_running_service_untouched(self):
        rp=FakeRP([dict(self.states()[0],active=1,stopped=0)],'')
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(p.CollectionError):service.prime(rp,b'x'*512,Path(d))
        self.assertEqual(rp.sent,[])
    def test_wrong_identity_never_armed(self):
        rp=FakeRP(self.states(),'wrong')
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(p.CollectionError):service.prime(rp,b'x'*512,Path(d))
        self.assertEqual(rp.sent,['R','q'])

class Firmware(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.check=module('ramfw','verify-ramsetup-firmware.py')
        cls.fw=Path(os.environ.get('RAMSETUP_FIRMWARE_DIR','/tmp/visor-ramsetup-p2-firmware'))
        base=ROOT.parent/'handspring-visor-plan/evidence/phase-3/2026-10-25-p03-011-persistent-reply'
        cls.elf=(cls.fw/'springboard_rom_persistent.elf').read_bytes()
        cls.bin=(cls.fw/'springboard_rom_persistent.bin').read_bytes()
        cls.uf2=(cls.fw/'springboard_rom_persistent.uf2').read_bytes()
        cls.baseline_elf=(base/'persistent-reply.elf').read_bytes();cls.baseline_uf2=(base/'persistent-reply.uf2').read_bytes()
        cls.rom=(BUILD/'ramsetup.rom').read_bytes()
    def verify(self,**kw):
        return self.check.verify(*(kw.get(k,getattr(self,k)) for k in ('elf','bin','uf2','baseline_elf','baseline_uf2','rom')))
    def test_unchanged_engine(self):self.assertTrue(self.verify()['critical_functions_equal'])
    def test_binary_mutation(self):
        b=bytearray(self.bin);b[300]^=1
        with self.assertRaisesRegex(ValueError,'BIN differs'):self.verify(bin=bytes(b))
    def test_uf2_payload_mutation(self):
        b=bytearray(self.uf2);b[512+32+60]^=1
        with self.assertRaisesRegex(ValueError,'payload differs'):self.verify(uf2=bytes(b))
    def test_wrong_family(self):
        b=bytearray(self.uf2);b[512+28]^=1
        with self.assertRaisesRegex(ValueError,'header/family'):self.verify(uf2=bytes(b))
    def test_absolute_block(self):
        b=bytearray(self.uf2);b[40]^=1
        with self.assertRaisesRegex(ValueError,'absolute'):self.verify(uf2=bytes(b))
    def test_wrong_embedded_image(self):
        b=bytearray(self.rom);b[400]^=1
        with self.assertRaisesRegex(ValueError,'embedded ROM'):self.verify(rom=bytes(b))
    def test_critical_kernel_mutation(self):
        e=self.check.ELF(self.elf);v,n,kind,index=e.symbols['sb_persistent_reply'];s=e.sections[index]
        b=bytearray(self.elf);off=s[4]+(v&~1)-s[3];b[off]^=1
        # Update BIN/UF2 not needed: the independent ELF/BIN comparison fails
        # first, so test the function bytes directly against the baseline too.
        self.assertNotEqual(self.check.ELF(bytes(b)).symbol('sb_persistent_reply'),self.check.ELF(self.baseline_elf).symbol('sb_persistent_reply'))
        with self.assertRaises(ValueError):self.verify(elf=bytes(b))

class Bundle(unittest.TestCase):
    def test_hash_gate_and_fallback(self):
        import hashlib
        with tempfile.TemporaryDirectory() as d:
            path=Path(d);items={}
            for name,data in [('rom',b'x'*512),('uf2',b'candidate'),('static',b'{}'),('oldrom',b'y'*512),('olduf2',b'baseline')]:
                (path/name).write_bytes(data);items[name]=dict(path=name,sha256=hashlib.sha256(data).hexdigest())
            m=dict(kind='ramsetup-p2',responder=dict(critical_functions_equal=True),artifacts={k:items[k] for k in ('rom','uf2','static')},fallback_artifacts=dict(rom=items['oldrom'],uf2=items['olduf2']))
            file=path/'bundle.json';file.write_text(json.dumps(m))
            self.assertEqual(service.load_bundle(file)[1]['rom'],b'x'*512)
            self.assertEqual(service.load_bundle(file,True)[1]['rom'],b'y'*512)
            (path/'uf2').write_bytes(b'tampered')
            with self.assertRaisesRegex(ValueError,'hash mismatch'):service.load_bundle(file)

if __name__=='__main__':unittest.main()
