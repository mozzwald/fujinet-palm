import contextlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))

import qualified_rom_readback as q
import prism_collect as p
import paired_collect as c
import test_paired_collect as pair_fixtures


def prc():
    header=bytearray(78);header[:32]=b'Test FNVD'.ljust(32,b'\0')
    struct.pack_into('>H',header,32,1);header[60:68]=b'applFNVD';struct.pack_into('>H',header,76,1)
    return bytes(header)+struct.pack('>4sHI',b'code',1,90)+b'\0\0'+b'qualified code'


class QualifiedReadback(unittest.TestCase):
    def setUp(self):
        self.fixture=pair_fixtures.PairedResults();self.fixture.setUp()
        self.tmp=tempfile.TemporaryDirectory();self.root=Path(self.tmp.name)
        self.source=self.root/'pairs';self.source.mkdir();self.rom=self.fixture.rom;self.prc=prc();self.firmware='11'*32
        (self.source/'reference.rom').write_bytes(self.rom);(self.source/'paired-checker.prc').write_bytes(self.prc)
        state=dict(profile='paired-move-l-v1',transport='palm-sync',installed=True,transport_verified=True,
            pending=None,active_firmware_sha256=self.firmware,rom_sha256=q.sha(self.rom),prc_sha256=q.sha(self.prc),
            session=self.fixture.session,previous_pdb='latest.pdb',validation=[])
        records=[];seq=0
        for offset in c.patterns(self.rom).values():
            for count in (4,100,10000):
                seq+=1;value=int.from_bytes(self.rom[offset:offset+4],'big')
                records.extend([self.fixture.record(status=0,count=count,offset=offset,sequence=seq),
                    self.fixture.record([value]*count,status=1,count=count,offset=offset,sequence=seq)])
                state['validation'].append(dict(sequence=seq,offset=offset,count=count,qualified_pass=True,firmware_sha256=self.firmware))
                run=self.source/f'run-{seq:06d}-fixture';run.mkdir()
                data=dict(run=seq,stopped=1,active=0,arm_pending=0,counters_final=1,requests=count*2,posted=count*2,
                    completed=count*2,attempted=count*2,cancelled=0,empty_ends=0)
                (run/'rp-final.json').write_text(json.dumps(data));(run/'rp-armed.json').write_text(json.dumps(dict(run=seq)))
                (run/'rp-config.json').write_text(json.dumps(dict(engine='continuous-v3',marker='preload',response='cpu-rom',words=len(self.rom)//2)))
        (self.source/'latest.pdb').write_bytes(p.make_pdb('ROMPairResults',records))
        (self.source/'session.json').write_text(json.dumps(state))

    def tearDown(self):
        self.fixture.tearDown();self.tmp.cleanup()

    def prepare(self):
        directory=self.root/'full'
        args=type('Args',(),dict(directory=directory,paired_session=self.source,firmware_sha256=self.firmware,
                               authorization='User requested one full read then manual recognition'))()
        with contextlib.redirect_stdout(io.StringIO()):q.prepare(args)
        return directory

    def test_qualification_checks_actual_results_counters_and_all_six_gates(self):
        _,_,proof=q.qualification(self.source,self.firmware);self.assertEqual(len(proof['gates']),6)
        with self.assertRaises(p.CollectionError):q.qualification(self.source,'22'*32)
        target=self.source/'run-000006-fixture/rp-final.json';data=json.loads(target.read_text());data['cancelled']=1;target.write_text(json.dumps(data))
        with self.assertRaises(p.CollectionError):q.qualification(self.source,self.firmware)

    def test_missing_gate_and_mutated_qualification_rejected(self):
        directory=self.prepare();q.load(directory)
        proof=directory/'qualification.json';proof.write_text(proof.read_text()+' ')
        with self.assertRaises(p.CollectionError):q.load(directory)
        state=json.loads((self.source/'session.json').read_text());state['validation'].pop();(self.source/'session.json').write_text(json.dumps(state))
        with self.assertRaises(p.CollectionError):q.qualification(self.source,self.firmware)

    def test_resource_match_tolerates_open_header_but_rejects_wrong_code_reset_and_creator(self):
        actual=bytearray(self.prc);actual[32:34]=(0x8001).to_bytes(2,'big');self.assertEqual(q.resources(actual),q.resources(self.prc))
        actual[-1]^=1;self.assertNotEqual(q.resources(actual),q.resources(self.prc))
        actual[32:34]=(0x21).to_bytes(2,'big')
        with self.assertRaises(p.CollectionError):q.resources(actual)
        with self.assertRaises(p.CollectionError):q.resources(self.prc[:85])

    def test_connect_roundtrips_control_and_only_marks_verified_after_both_checks(self):
        directory=self.prepare();directory,state,_=q.load(directory)
        def transfer(command,**kwargs):
            output=Path(command[-1]);(output/'installed-app.prc').write_bytes(self.prc)
            (output/'ROMProbeControl.pdb').write_bytes((directory/'ROMProbeControl.pdb').read_bytes())
            return type('Proc',(),{'returncode':0})()
        with patch.object(p,'wait_palm',return_value='/dev/ttyUSB1'),patch.object(q.subprocess,'run',side_effect=transfer),contextlib.redirect_stdout(io.StringIO()):
            q.connect(directory,state,1)
        self.assertTrue(json.loads((directory/'session.json').read_text())['transport_verified'])
        output=next(directory.glob('control-roundtrip-*'));command=json.loads((output/'command.json').read_text())
        self.assertEqual(Path(command[1]).name,'control_roundtrip.cjs');self.assertEqual(command[2],'/dev/ttyUSB1')

    def test_readback_gate_requires_verified_payload_and_matching_firmware(self):
        directory=self.prepare();_,state,rom=q.load(directory)
        with self.assertRaises(p.CollectionError):q.readback_gate(directory,rom,self.firmware)
        state['last_sequence']=1;(directory/'session.json').write_text(json.dumps(state));run=directory/'run-000001-fixture';run.mkdir()
        report=dict(result='PASS',firmware_sha256=self.firmware,payload_sha256=q.sha(rom),completed=len(rom),expected_size=len(rom),mismatch_count=0,
            rp=dict(requests=len(rom)//2,posted=len(rom)//2,completed=len(rom)//2,attempted=len(rom)//2,stopped=1,counters_final=1,
                    cancelled=0,empty_ends=0,active=0,arm_pending=0),rp_config=dict(engine='continuous-v3',marker='preload'))
        (run/'report.json').write_text(json.dumps(report));(run/'readback.rom').write_bytes(rom)
        q.readback_gate(directory,rom,self.firmware)
        report['rp']['completed']-=1;(run/'report.json').write_text(json.dumps(report))
        with self.assertRaises(p.CollectionError):q.readback_gate(directory,rom,self.firmware)
        report['rp']['completed']+=1;(run/'report.json').write_text(json.dumps(report))
        (run/'readback.rom').write_bytes(rom[:-1]+b'x')
        with self.assertRaises(p.CollectionError):q.readback_gate(directory,rom,self.firmware)

    def test_one_read_limit_blocks_rearm_before_opening_hardware(self):
        directory=self.prepare();directory,state,rom=q.load(directory)
        state['transport_verified']=True;state['last_sequence']=1
        with patch.object(p,'ContinuousRP') as ctor:
            with self.assertRaises(p.CollectionError):q.run(directory,state,rom,object())
        ctor.assert_not_called()

    def test_report_requires_persistent_marker_and_stopped_final_totals(self):
        # Use the existing ROM result fixture; only responder metadata changes.
        from test_prism_collect import Records
        f=Records();f.setUp();d=p.decode_record(f.pair(kind=1)[1],f.rom);n=len(f.rom)//2
        d['rp_armed']=dict(run=1,continuous=1,limit=0)
        d['rp_config']=dict(engine='continuous-v3',marker='preload',response='cpu-rom',words=n,continuous=True)
        final=dict(run=1,continuous=1,limit=0,stopped=1,active=0,arm_pending=0,counters_final=1,
                   requests=n,posted=n,completed=n,attempted=n,cancelled=0,empty_ends=0)
        self.assertEqual(p.report(self.root,d,final)['result'],'PASS')
        final['counters_final']=0;self.assertEqual(p.report(self.root,d,final)['result'],'COLLECTION ERROR')
        final['counters_final']=1;d['rp_config']['marker']='drive';self.assertEqual(p.report(self.root,d,final)['result'],'COLLECTION ERROR')
