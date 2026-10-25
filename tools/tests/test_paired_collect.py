import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
import tempfile

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import paired_collect as c
import prism_collect as p

class PairedResults(unittest.TestCase):
    def setUp(self):
        c.configure()
        self.rom=bytes.fromhex('0000300000000000feedbeef0000ffff')+bytes(512)
        self.session='12'*16

    def tearDown(self):
        p.configure_profile('baseline')

    def record(self,values=(),status=0,count=4,offset=8,**changes):
        expected=int.from_bytes(self.rom[offset:offset+4],'big')
        data=b''.join(struct.pack('>I',v) for v in values)
        d=dict(sequence=1,status=status,offset=offset,expected=expected,requested=count,
            completed=len(values),first_bad=sum(v>>16!=expected>>16 for v in values),
            second_bad=sum((v&65535)!=(expected&65535) for v in values),
            long_bad=sum(v!=expected for v in values),base=0x28000000,size=0x100000,
            before=330,after=330 if status else 0,reason=0,error=0,started=1,
            ended=2 if status else 0,payload_crc=p.crc(data),reference_crc=p.crc(self.rom),
            app_version=0x700,length=c.HEADER+len(data)+4,header_size=c.HEADER,
            opcodes_per_value=1,interpair_ticks=1,reserved1=0,reserved2=0)
        d.update(changes)
        body=(b'VPL1'+struct.pack('>I',1)+bytes.fromhex(self.session)+hashlib.sha256(self.rom).digest()+
              struct.pack('>26I',*(d[k] for k in c.FIELDS))+data)
        return body+struct.pack('>I',p.crc(body))

    def test_first_second_and_both_failures(self):
        values=[0xfeedbeef,0xfeed0000,0x0000beef,0]
        d=c.decode_record(self.record(values,2),self.rom)
        self.assertEqual((d['first_correct'],d['first_bad'],d['second_correct'],d['second_bad'],d['long_bad']),(2,2,2,2,3))
        self.assertEqual(d['mismatches'][0]['actual'],'FEED0000')
        self.assertEqual(d['mismatches'][1]['second_actual'],'BEEF')
        self.assertEqual(d['values_hex'],['FEEDBEEF','FEED0000','0000BEEF','00000000'])

    def test_pass(self):
        self.assertEqual(c.decode_record(self.record([0xfeedbeef]*4,1),self.rom)['result'],'PASS')

    def test_all_10000_failures_retained(self):
        d=c.decode_record(self.record([0]*10000,2,count=10000),self.rom)
        self.assertEqual(len(d['mismatches']),10000)
        self.assertEqual(d['first_bad'],10000)
        self.assertEqual(d['second_bad'],10000)
        self.assertEqual(d['mismatches'][-1]['index'],9999)

    def test_partial_cancel_retains_prefix(self):
        d=c.decode_record(self.record([0xfeedbeef],3),self.rom)
        self.assertEqual(d['completed'],1)
        self.assertEqual(d['result'],'INCOMPLETE')

    def test_reject_forged_counts_status_timing_and_metadata(self):
        for changes in [dict(first_bad=1),dict(second_bad=2),dict(long_bad=1),dict(after=390),
                        dict(expected=0),dict(reference_crc=0),dict(opcodes_per_value=2),
                        dict(interpair_ticks=0),dict(header_size=144),dict(reserved2=1),
                        dict(error=1),dict(reason=1),dict(app_version=0x601)]:
            with self.subTest(changes=changes),self.assertRaises(p.CollectionError):
                c.decode_record(self.record([0xfeedbeef]*4,1,**changes),self.rom)
        with self.assertRaises(p.CollectionError):c.decode_record(self.record([0]*4,1),self.rom)
        with self.assertRaises(p.CollectionError):c.decode_record(self.record([0xfeedbeef]*4,2),self.rom)

    def test_reject_corrupt_payload(self):
        raw=bytearray(self.record([0xfeedbeef]*4,1));raw[-6]^=1
        with self.assertRaises(p.CollectionError):c.decode_record(bytes(raw),self.rom)

    def test_start_final_pair(self):
        data=p.make_pdb(c.DB,[self.record(),self.record([0xfeedbeef]*4,1)])
        self.assertEqual(c.decode_pdb(data,self.session,self.rom)[1]['final']['result'],'PASS')

    def test_missing_duplicate_or_mismatched_markers(self):
        for records in [[self.record([0xfeedbeef]*4,1)],
                        [self.record(),self.record()],
                        [self.record(),self.record([0xfeedbeef]*4,1,started=4)]]:
            with self.assertRaises(p.CollectionError):
                c.decode_pdb(p.make_pdb(c.DB,records),self.session,self.rom)

    def test_pattern_is_existing_aligned_maximum_contrast(self):
        self.assertEqual(c.patterns(self.rom),{'signature':8,'contrast':12})

    def test_bad_signature(self):
        with self.assertRaises(p.CollectionError):c.patterns(bytes(512))

    def test_larger_runs_require_same_pattern_qualified_smaller_run(self):
        c.check_escalation({'validation':[]},8,4)
        for count,previous in [(100,4),(10000,100)]:
            for v in [[],[dict(offset=8,count=previous,qualified_pass=False)],
                         [dict(offset=12,count=previous,qualified_pass=True)]]:
                with self.assertRaises(p.CollectionError):c.check_escalation({'validation':v},8,count)
            c.check_escalation({'validation':[dict(offset=8,count=previous,qualified_pass=True)]},8,count)
        with self.assertRaises(p.CollectionError):
            c.check_escalation({'active_firmware_sha256':'new',
                'validation':[dict(offset=8,count=4,qualified_pass=True,firmware_sha256='old')]},8,100)

    def test_missing_requests_preserve_failed_payload_in_report(self):
        d=c.decode_record(self.record([0xfeed0000]*4,2),self.rom)
        armed=dict(run=1)
        final=dict(run=1,stopped=1,active=0,arm_pending=0,limit=0,continuous=1,
            requests=4,posted=4,completed=4,attempted=4,cancelled=0,empty_ends=0)
        with tempfile.TemporaryDirectory() as tmp:
            r=c.report(Path(tmp),d,armed,final)
            self.assertFalse(r['counter_match']);self.assertFalse(r['qualified_pass'])
            self.assertEqual(len(r['mismatches']),4)
            self.assertIn('FEED0000',(Path(tmp)/'mismatches.csv').read_text())

if __name__=='__main__':unittest.main()
