import sys
from pathlib import Path
import struct
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import paired_collect as c
import prism_collect as p
import test_paired_collect as fixtures

class WordResults(unittest.TestCase):
    def setUp(self):
        self.helper=fixtures.PairedResults();self.helper.setUp()
        self.rom=self.helper.rom
        c.WORD_MODE=True;c.configure()

    def tearDown(self):
        c.WORD_MODE=False;c.configure();p.configure_profile('baseline')

    def record(self,nops=0,status=1,**kw):
        raw=bytearray(self.helper.record([0xfeedbeef]*4 if status else [],status,app_version=0x800,
            opcodes_per_value=2,reserved1=1,reserved2=nops,**kw))
        raw[:4]=b'VPW1';raw[-4:]=struct.pack('>I',p.crc(raw[:-4]));return bytes(raw)

    def test_every_nop_value(self):
        for n in c.NOP_COUNTS:
            d=c.decode_record(self.record(n),self.rom)
            self.assertEqual(d['nop_count'],n);self.assertEqual(d['instruction_mode'],'two-MOVE.W')
            self.assertEqual(d['values_hex'],['FEEDBEEF']*4)

    def test_isolated_database_creator(self):
        self.assertEqual((c.DB,c.CONTROL,p.DB_CREATOR),('ROMWordResults','ROMWordControl',b'FNVE'))
        with self.assertRaises(p.CollectionError):c.decode_record(self.helper.record(),self.rom)
        raw=self.record();c.WORD_MODE=False;c.configure()
        with self.assertRaises(p.CollectionError):c.decode_record(raw,self.rom)

    def test_reject_unsupported_spacing_and_large_count(self):
        for n in [3,63,65,0xffffffff]:
            with self.assertRaises(p.CollectionError):c.decode_record(self.record(n),self.rom)
        with self.assertRaises(p.CollectionError):c.decode_record(self.record(count=100),self.rom)

    def test_start_final_spacing_must_match(self):
        bad=p.make_pdb(c.DB,[self.record(0,status=0),self.record(1)])
        with self.assertRaises(p.CollectionError):c.decode_pdb(bad,self.helper.session,self.rom)
        good=p.make_pdb(c.DB,[self.record(2,status=0),self.record(2)])
        self.assertEqual(c.decode_pdb(good,self.helper.session,self.rom)[1]['final']['nop_count'],2)

    def test_wrong_instruction_count_rejected(self):
        raw=bytearray(self.record());struct.pack_into('>I',raw,144,1)
        raw[-4:]=struct.pack('>I',p.crc(raw[:-4]))
        with self.assertRaises(p.CollectionError):c.decode_record(bytes(raw),self.rom)

if __name__=='__main__':unittest.main()
