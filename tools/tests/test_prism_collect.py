import contextlib
import importlib.util
import io
import json
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

spec=importlib.util.spec_from_file_location('prism',Path(__file__).resolve().parents[1]/'prism_collect.py')
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)

class Records(unittest.TestCase):
    def setUp(self):
        self.rom=bytes(range(256))*28+bytes(range(80))
        self.session='0123456789abcdef'*2

    def pair(self, actual=None, status=1, kind=2, seq=1):
        if actual is None: actual=self.rom
        return [m.fixture_record(self.session,self.rom,seq,kind,0),
                m.fixture_record(self.session,self.rom,seq,kind,status,actual)]

    def test_pass_and_every_failed_word(self):
        data=m.make_pdb(m.DB,self.pair())
        self.assertEqual(m.decode_pdb(data,self.session,self.rom)[1]['result'],'PASS')
        bad=bytes(b^(i&1) for i,b in enumerate(self.rom))
        d=m.decode_pdb(m.make_pdb(m.DB,self.pair(bad,2,3)),self.session,self.rom)[1]
        self.assertEqual(d['result'],'FAIL');self.assertEqual(d['mismatch_count'],len(self.rom)//2)
        self.assertEqual(d['mismatches'][-1]['offset'],len(self.rom)-2)

    def test_pdb_record_offsets_allow_optional_padding(self):
        data=bytearray(m.make_pdb(m.DB,self.pair()))
        count=struct.unpack_from('>H',data,76)[0]
        end=78+8*count
        del data[end:end+2]
        for i in range(count):
            off=78+8*i
            struct.pack_into('>I',data,off,struct.unpack_from('>I',data,off)[0]-2)
        self.assertEqual(m.decode_pdb(data,self.session,self.rom)[1]['result'],'PASS')

    def test_partial_and_cancelled_remain_incomplete(self):
        for records in ([self.pair()[0]],self.pair(self.rom[:200],3)):
            d=m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)[1]
            self.assertEqual(d['result'],'INCOMPLETE')

    def test_serial_export_open_flag_keeps_payload_validation(self):
        data=bytearray(m.make_pdb(m.DB,self.pair()))
        struct.pack_into('>H',data,32,0x8000)
        self.assertEqual(m.decode_pdb(data,self.session,self.rom)[1]['result'],'PASS')
        resource=bytearray(data);struct.pack_into('>H',resource,32,0x8001)
        with self.assertRaises(m.CollectionError):m.decode_pdb(resource,self.session,self.rom)
        data[-1]^=1
        with self.assertRaises(m.CollectionError):m.decode_pdb(data,self.session,self.rom)

    def test_corruption_and_truncation_rejected(self):
        raw=m.make_pdb(m.DB,self.pair())
        for index in (0,60,64,78,len(raw)-1,len(raw)-20):
            bad=bytearray(raw);bad[index]^=1
            with self.assertRaises(m.CollectionError):m.decode_pdb(bad,self.session,self.rom)
        for length in (0,77,79,len(raw)-1):
            with self.assertRaises(m.CollectionError):m.decode_pdb(raw[:length],self.session,self.rom)

    def test_stale_session_duplicate_and_missing_markers(self):
        for records in (self.pair()[1:],self.pair()+self.pair()[1:],self.pair()+self.pair()[:1]):
            with self.assertRaises(m.CollectionError):m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)
        with self.assertRaises(m.CollectionError):
            m.decode_pdb(m.make_pdb(m.DB,self.pair()),'ff'*16,self.rom)

    def test_forged_claim_and_lost_mismatch_rejected(self):
        bad=bytearray(self.rom);bad[20]^=1
        records=self.pair(bytes(bad),1)
        with self.assertRaises(m.CollectionError):m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)
        final=bytearray(self.pair(bytes(bad),2)[1]);final[-5]^=1
        final[-4:]=struct.pack('>I',m.crc(final[:-4]))
        with self.assertRaises(m.CollectionError):m.decode_pdb(m.make_pdb(m.DB,[self.pair()[0],final]),self.session,self.rom)

    def test_version_and_reference_identity(self):
        for offset in (4,24,76,132,136,140):
            records=self.pair();bad=bytearray(records[1]);bad[offset]^=1
            bad[-4:]=struct.pack('>I',m.crc(bad[:-4]));records[1]=bytes(bad)
            with self.assertRaises(m.CollectionError):m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)

    def test_whole_session_history_survives(self):
        records=[]
        for seq in range(1,6):records+=self.pair(seq=seq)
        runs=m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)
        self.assertEqual(list(runs),[1,2,3,4,5])

    def test_maintenance_app_version_preserves_old_history(self):
        records=self.pair()
        for raw in self.pair(seq=2):
            updated=bytearray(raw)
            struct.pack_into('>I',updated,132,0x401)
            updated[-4:]=struct.pack('>I',m.crc(updated[:-4]))
            records.append(bytes(updated))
        runs=m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)
        self.assertEqual(runs[1]['app_version'],0x400)
        self.assertEqual(runs[2]['app_version'],0x401)
        self.assertEqual(runs[2]['result'],'PASS')
        unsupported=bytearray(records[-1]);struct.pack_into('>I',unsupported,132,0x402)
        unsupported[-4:]=struct.pack('>I',m.crc(unsupported[:-4]))
        with self.assertRaises(m.CollectionError):m.decode_record(unsupported,self.rom)

    def test_c_encoder_cross_language(self):
        repo=Path(__file__).resolve().parents[2]
        executable=repo/'palm/apps/visorcheck/tests/build/test_codec'
        if not executable.exists():self.fail('run make -C palm/apps/visorcheck check first')
        with tempfile.TemporaryDirectory() as tmp:
            subprocess.run([str(executable),tmp],check=True,stdout=subprocess.PIPE)
            rom=bytes(range(256))
            for name,status in [('pass',1),('fail',2),('cancel',3),('start',0)]:
                raw=(Path(tmp)/(name+'.bin')).read_bytes()
                d=m.decode_record(raw,rom)
                self.assertEqual(d['status'],status)
                if name=='fail':self.assertEqual(d['mismatch_count'],128)
            for kind in (4,5,6):
                d=m.decode_record((Path(tmp)/f'constant{kind}.bin').read_bytes(),rom)
                self.assertEqual(d['kind'],kind);self.assertEqual(d['completed'],128)
                self.assertEqual(d['result'],'PASS')
            for mode, label in enumerate(m.READ_SPACING):
                for profile, name in enumerate(m.CAPTURE_PROFILES):
                    d=m.decode_record((Path(tmp)/f'spacing_{mode}_{profile}.bin').read_bytes(),rom)
                    self.assertEqual((d['read_spacing'],d['capture_profile']),(label,name))
                    self.assertEqual(d['batch_words'],64 if profile else 1)
                    self.assertEqual(d['delay_iterations'],m.SPACING_ITERATIONS[mode])

    def test_spacing_history_and_exact_pairing(self):
        records=self.pair()
        for seq, label in enumerate(m.READ_SPACING,2):
            for status in (0,1):
                records.append(m.fixture_record(self.session,self.rom,seq,6,status,
                    self.rom[:128] if status else b'',label,'burst64'))
        runs=m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)
        self.assertEqual(len(runs),6)
        self.assertEqual(runs[1]['record_version'],1)
        self.assertEqual(runs[6]['read_spacing'],'none')
        wrong=list(records)
        wrong[-1]=m.fixture_record(self.session,self.rom,6,6,1,self.rom[:128],'1us','burst64')
        with self.assertRaises(m.CollectionError):
            m.decode_pdb(m.make_pdb(m.DB,wrong),self.session,self.rom)

    def test_spacing_metadata_corruption_with_valid_crc(self):
        raw=m.fixture_record(self.session,self.rom,1,6,1,self.rom[:128],'1us','burst64')
        for offset, value in ((144,99),(148,99),(152,0),(156,1),(132,0x500),(140,144)):
            bad=bytearray(raw);struct.pack_into('>I',bad,offset,value)
            bad[-4:]=struct.pack('>I',m.crc(bad[:-4]))
            with self.assertRaises(m.CollectionError):m.decode_record(bad,self.rom)

    def test_optimized_checker_version_and_pair_identity(self):
        records=[]
        for seq, version in ((1,0x600),(2,0x601)):
            for status in (0,1):
                raw=bytearray(m.fixture_record(self.session,self.rom,seq,6,status,
                    self.rom[:128] if status else b'','none','burst64'))
                struct.pack_into('>I',raw,132,version)
                raw[-4:]=struct.pack('>I',m.crc(raw[:-4]))
                records.append(bytes(raw))
        runs=m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)
        self.assertEqual(runs[1]['app_version'],0x600)
        self.assertEqual(runs[2]['app_version'],0x601)
        bad=bytearray(records[-1]);struct.pack_into('>I',bad,132,0x600)
        bad[-4:]=struct.pack('>I',m.crc(bad[:-4]));records[-1]=bytes(bad)
        with self.assertRaises(m.CollectionError):
            m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)

class Workflow(unittest.TestCase):
    setUp=Records.setUp
    pair=Records.pair
    def state(self, root, kind='synthetic-pass'):
        output=root/'run-000001';output.mkdir()
        state=dict(session=self.session,last_sequence=0,pending=dict(sequence=1,kind=kind,directory=output.name))
        return state,output

    def fake_transfer(self, data):
        def transfer(session, output, mode, names, timeout, **kwargs):
            (output/(m.DB+'.pdb')).write_bytes(data)
        return transfer

    def test_collect_requires_matching_sequence_and_kind(self):
        for records in (self.pair(seq=2),self.pair(kind=3)):
            with tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);state,output=self.state(root)
                with patch.object(m,'transfer',side_effect=self.fake_transfer(m.make_pdb(m.DB,records))):
                    with self.assertRaises(m.CollectionError):m.collect(root,state,self.rom,1)
                self.assertIsNotNone(state['pending']);self.assertEqual(state['last_sequence'],0)
                self.assertTrue(list(output.glob('download-*/*.pdb')))

    def test_transfer_failure_does_not_ack_or_destroy_pending(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);state,_=self.state(root)
            with patch.object(m,'transfer',side_effect=m.CollectionError('missed HotSync')):
                with self.assertRaises(m.CollectionError):m.collect(root,state,self.rom,1)
            self.assertIsNotNone(state['pending']);self.assertEqual(state['last_sequence'],0)

    def test_successful_retry_and_preserved_raw_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);state,output=self.state(root)
            with patch.object(m,'transfer',side_effect=self.fake_transfer(m.make_pdb(m.DB,self.pair()))):
                with contextlib.redirect_stdout(io.StringIO()):self.assertEqual(m.collect(root,state,self.rom,1),0)
            self.assertIsNone(state['pending']);self.assertEqual(state['last_sequence'],1)
            self.assertTrue((output/'report.json').exists());self.assertTrue(list(output.glob('download-*/*.pdb')))

    def test_rom_without_rp_cannot_pass(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);state,output=self.state(root,'rom')
            with patch.object(m,'transfer',side_effect=self.fake_transfer(m.make_pdb(m.DB,self.pair(kind=1)))):
                with self.assertRaises(m.CollectionError):m.collect(root,state,self.rom,1)
            self.assertEqual(json.loads((output/'report.json').read_text())['result'],'COLLECTION ERROR')
            self.assertIsNotNone(state['pending'])

    def test_rp_run_and_budget_validation(self):
        d=m.decode_record(self.pair(kind=1)[1],self.rom)
        armed=dict(run=12,limit=len(self.rom)//2)
        d['rp_armed']=armed
        rp=dict(run=12,limit=armed['limit'],requests=armed['limit'],posted=armed['limit'],
                completed=armed['limit'],enabled=armed['limit'],not_enabled=0,address_errors=0,
                anomalies=0,stopped=1,arm_pending=0)
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(m.report(Path(tmp),d,rp)['result'],'PASS')
            rp['run']=13
            self.assertEqual(m.report(Path(tmp),d,rp)['result'],'COLLECTION ERROR')
            rp['run']=12;rp['completed']-=1
            self.assertEqual(m.report(Path(tmp),d,rp)['result'],'INCOMPLETE')

    def test_probe_checks_contents_after_successful_process(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc';rom.write_bytes(self.rom);prc.write_bytes(b'prc')
            out=root/'session'
            with contextlib.redirect_stdout(io.StringIO()):self.assertEqual(m.main(['setup',str(out),'--rom',str(rom),'--prc',str(prc)]),0)
            state=json.loads((out/'session.json').read_text());state['installed']=True;m.atomic_json(out/'session.json',state)
            def wrong(session,output,mode,names,timeout,**kwargs):(output/'VisorCheckControl.pdb').write_bytes(b'not a PDB')
            with patch.object(m,'transfer',side_effect=wrong),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(m.main(['probe',str(out)]),2)
            self.assertFalse(json.loads((out/'session.json').read_text())['transport_verified'])

    def test_probe_accepts_serial_open_header_only_with_exact_control(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc'
            rom.write_bytes(self.rom);prc.write_bytes(b'prc');out=root/'session'
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(m.main(['setup',str(out),'--rom',str(rom),'--prc',str(prc)]),0)
            state=json.loads((out/'session.json').read_text())
            state['installed']=True;state['transport']='palm-sync';m.atomic_json(out/'session.json',state)
            payload=bytearray((out/'VisorCheckControl.pdb').read_bytes())
            struct.pack_into('>H',payload,32,0x8000)
            def fetched(session,output,mode,names,timeout,**kwargs):
                self.assertEqual(kwargs['transport'],'palm-sync')
                (output/'VisorCheckControl.pdb').write_bytes(payload)
            with patch.object(m,'transfer',side_effect=fetched),contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(m.main(['probe',str(out)]),0)
            state=json.loads((out/'session.json').read_text());self.assertTrue(state['transport_verified'])
            state['transport_verified']=False;m.atomic_json(out/'session.json',state)
            payload[-1]^=1
            with patch.object(m,'transfer',side_effect=fetched),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(m.main(['probe',str(out)]),2)
            self.assertFalse(json.loads((out/'session.json').read_text())['transport_verified'])

    def test_pilot_transfer_error_and_timeout(self):
        for value in (subprocess.CompletedProcess([],1),subprocess.TimeoutExpired([],1)):
            with tempfile.TemporaryDirectory() as tmp:
                with patch.object(m,'wait_palm',return_value='/dev/ttyUSB1'),patch.object(m.shutil,'which',return_value='/usr/bin/pilot-xfer'):
                    kwargs={'side_effect':value} if isinstance(value,Exception) else {'return_value':value}
                    with patch.object(m.subprocess,'run',**kwargs):
                        with self.assertRaises(m.CollectionError):m.transfer(Path(tmp),Path(tmp),'--fetch',[m.DB],1)

    def test_five_simulated_full_workflows_and_retrieval_retry(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc'
            rom.write_bytes(self.rom);prc.write_bytes(b'prc');directory=root/'session'
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(m.main(['setup',str(directory),'--rom',str(rom),'--prc',str(prc)]),0)
            history=[]
            def device_transfer(session,output,mode,names,timeout,**kwargs):
                state=json.loads((directory/'session.json').read_text())
                if mode=='--install':return
                if names==['VisorCheckControl']:
                    (output/'VisorCheckControl.pdb').write_bytes((directory/'VisorCheckControl.pdb').read_bytes());return
                pending=state['pending'];seq=pending['sequence'];kind=m.KIND[pending['kind']]
                actual=self.rom if kind==2 else bytes(b^(i&1) for i,b in enumerate(self.rom))
                history.extend([m.fixture_record(state['session'],self.rom,seq,kind,0),
                                m.fixture_record(state['session'],self.rom,seq,kind,1 if kind==2 else 2,actual)])
                (output/(m.DB+'.pdb')).write_bytes(m.make_pdb(m.DB,history))
            with patch.object(m,'transfer',side_effect=device_transfer),contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(m.main(['install',str(directory)]),0)
                self.assertEqual(m.main(['probe',str(directory)]),0)
                for seq in range(1,6):
                    kind='synthetic-pass' if seq%2 else 'synthetic-fail'
                    self.assertEqual(m.main(['run',str(directory),'--kind',kind]),0 if seq%2 else 1)
                    state=json.loads((directory/'session.json').read_text())
                    self.assertEqual(state['last_sequence'],seq);self.assertIsNone(state['pending'])
            self.assertEqual(len(list(directory.glob('run-*/report.json'))),5)
            self.assertEqual(len(list(directory.glob('run-*/download-*/*.pdb'))),5)
            with patch.object(m,'transfer',side_effect=m.CollectionError('disconnected')),contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(m.main(['run',str(directory),'--kind','synthetic-pass']),2)
            pending=json.loads((directory/'session.json').read_text())['pending']
            self.assertEqual(pending['sequence'],6)
            with patch.object(m,'transfer',side_effect=device_transfer),contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(m.main(['collect',str(directory)]),0)
            self.assertEqual(json.loads((directory/'session.json').read_text())['last_sequence'],6)

    def test_fallback_uses_existing_serial_pull_and_still_checks_artifact(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            with patch.object(m,'wait_palm',return_value='/dev/ttyUSB1'),patch.object(m.Path,'exists',return_value=True),patch.object(m.shutil,'which',return_value='/usr/bin/node'):
                with patch.object(m.subprocess,'run',return_value=subprocess.CompletedProcess([],0)) as run:
                    m.transfer(root,root,'--fetch',[m.DB],1,transport='palm-sync')
                args=run.call_args.args[0]
                self.assertIn('pull',args);self.assertIn('--serial',args)
                self.assertNotIn('--usb',args);self.assertIn(m.DB,args)

    def test_preflight_failure_does_not_reserve_a_palm_sequence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc'
            rom.write_bytes(self.rom);prc.write_bytes(b'prc');directory=root/'session'
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(m.main(['setup',str(directory),'--rom',str(rom),'--prc',str(prc)]),0)
            state=json.loads((directory/'session.json').read_text());state['transport_verified']=True
            state['validation']=[dict(kind=2 if i%2 else 3) for i in range(5)]
            m.atomic_json(directory/'session.json',state)
            with patch.object(m,'rp_port',return_value='/dev/ttyACM0'),patch.object(m,'RP',side_effect=m.CollectionError('console busy')),contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(m.main(['run',str(directory)]),2)
            state=json.loads((directory/'session.json').read_text())
            self.assertIsNone(state['pending']);self.assertEqual(state['last_sequence'],0)

    def test_rp_arming_requires_fresh_run_zero_counts_and_exact_budget(self):
        rp=m.RP.__new__(m.RP)
        before=dict(run=8,rom_ready=1,ready=1,stopped=1,arm_pending=0)
        armed=dict(run=9,stopped=0,arm_pending=0,limit=3624,requests=0,posted=0,completed=0,enabled=0,not_enabled=0,address_errors=0,anomalies=0)
        with patch.object(rp,'send'),patch.object(rp,'status',return_value=armed):
            self.assertEqual(rp.arm(before,3624),armed)
        for field,value in [('run',8),('stopped',1),('limit',8),('requests',1),('arm_pending',1),('anomalies',1)]:
            bad=dict(armed);bad[field]=value
            with patch.object(rp,'send'),patch.object(rp,'status',return_value=bad):
                with self.assertRaises(m.CollectionError):rp.arm(before,3624)

    def test_rp_anomalies_are_not_mislabeled_as_rom_crc_failure(self):
        d=m.decode_record(self.pair(kind=1)[1],self.rom)
        d['rp_armed']={'run':1,'limit':len(self.rom)//2}
        count=len(self.rom)//2
        rp=dict(run=1,limit=count,requests=count,posted=count,completed=count,enabled=count,
                stopped=1,arm_pending=0,not_enabled=0,address_errors=1,anomalies=1)
        with tempfile.TemporaryDirectory() as tmp:
            self.assertEqual(m.report(Path(tmp),d,rp)['result'],'COLLECTION ERROR')

    def test_rp_wait_rejects_changed_identity_and_timeout(self):
        rp=m.RP.__new__(m.RP)
        armed={'run':1,'limit':3624}
        with patch.object(rp,'status',return_value=dict(run=2,limit=3624,stopped=0,arm_pending=0)):
            with self.assertRaises(m.CollectionError):rp.wait(armed,1)
        with patch.object(rp,'status',return_value=dict(run=1,limit=3624,stopped=0,arm_pending=0)),patch.object(m.time,'sleep'):
            with self.assertRaises(m.CollectionError):rp.wait(armed,.001)

    def test_fast_read_hotsync_collects_partial_rp_counts(self):
        rp=m.RP.__new__(m.RP);armed=dict(run=1,limit=64)
        partial=dict(run=1,limit=64,stopped=0,arm_pending=0,completed=61)
        with patch.object(rp,'status',return_value=partial),patch.object(m,'find_palm',return_value='/fake'),patch.object(rp,'stop',return_value=dict(partial,stopped=1)) as stop,patch.object(rp,'send') as send,patch.object(rp,'read'):
            result=rp.wait(armed,1,collect_on_hotsync=True)
            self.assertEqual(result['completed'],61)
            self.assertEqual(result['host_stopped_for_hotsync'],1)
            stop.assert_called_once();send.assert_called_once_with('e')

class Latency(unittest.TestCase):
    setUp=Records.setUp
    pair=Records.pair

    def test_short_patterns_and_all_mismatches(self):
        for kind in (4,5,6):
            payload=m.expected_payload(self.rom,kind)
            self.assertEqual(len(payload),128)
            good=m.decode_pdb(m.make_pdb(m.DB,self.pair(payload,1,kind)),self.session,self.rom)[1]
            self.assertEqual(good['result'],'PASS');self.assertEqual(good['completed'],128)
            bad=bytes(x^255 for x in payload)
            d=m.decode_pdb(m.make_pdb(m.DB,self.pair(bad,2,kind)),self.session,self.rom)[1]
            self.assertEqual(d['mismatch_count'],64)
            self.assertEqual(d['mismatches'][-1]['offset'],126)
            self.assertEqual(d['reference_crc'],m.crc(self.rom))

    def test_new_and_old_history_and_metadata_rejection(self):
        records=self.pair(seq=1)
        records+=self.pair(m.expected_payload(self.rom,4),1,4,seq=2)
        self.assertEqual(len(m.decode_pdb(m.make_pdb(m.DB,records),self.session,self.rom)),2)
        for offset,value in ((132,0x401),(68,7248),(72,0),(76,0)):
            raw=bytearray(records[-1]);struct.pack_into('>I',raw,offset,value)
            raw[-4:]=struct.pack('>I',m.crc(raw[:-4]))
            with self.assertRaises(m.CollectionError):m.decode_record(raw,self.rom)

    def test_response_config_and_posted_evidence(self):
        for kind in (4,5,6):
            for response in ({4:('cpu-zero','pio-zero'),5:('cpu-ones','pio-ones'),6:('cpu-rom',)}[kind]):
                d=m.decode_record(self.pair(m.expected_payload(self.rom,kind),1,kind)[1],self.rom)
                d['rp_armed']={'run':1,'limit':64}
                d['rp_config']=dict(response=response,marker='drive',words=64)
                rp=dict(run=1,limit=64,requests=64,posted=0 if response.startswith('pio') else 64,
                        completed=64,enabled=64,stopped=1,arm_pending=0,
                        not_enabled=0,address_errors=0,anomalies=0)
                with tempfile.TemporaryDirectory() as tmp:
                    self.assertEqual(m.report(Path(tmp),d,rp)['result'],'PASS')
                    rp['posted']^=64
                    self.assertEqual(m.report(Path(tmp),d,rp)['result'],'COLLECTION ERROR')
                    rp['posted']^=64;del d['rp_config']
                    self.assertEqual(m.report(Path(tmp),d,rp)['result'],'COLLECTION ERROR')
                    d['rp_config']=dict(response='wrong',marker='drive',words=64)
                    self.assertEqual(m.report(Path(tmp),d,rp)['result'],'COLLECTION ERROR')

    def test_mode_profile_identity_readback(self):
        for response in ('cpu-rom','cpu-zero','cpu-ones','pio-zero','pio-ones'):
            for marker in m.MARKERS:
                rp=m.RP.__new__(m.RP);commands=[]
                def read(seconds):
                    return f'LATENCY response={response} marker={marker} words=64' if commands[-1]=='q' else ''
                with patch.object(rp,'_identify') as identity,patch.object(rp,'send',side_effect=commands.append),patch.object(rp,'read',side_effect=read),patch.object(rp,'status',return_value={'stopped':1}):
                    state,config=rp.identify_latency(self.rom,response,marker)
                    self.assertEqual(config,dict(response=response,marker=marker,words=64))
                    identity.assert_called_once_with(self.rom,'FujiSpring LATENCY DIAGNOSTIC / bounded reads / 150MHz')
                with patch.object(rp,'_identify'),patch.object(rp,'send'),patch.object(rp,'read',return_value='LATENCY response=cpu-rom marker=drive words=3624'):
                    with self.assertRaises(m.CollectionError):rp.identify_latency(self.rom,response,marker)

    def test_short_workflows_and_hotsync_retry(self):
        for kind,response in (('constant-zero','cpu'),('constant-zero','pio'),('constant-ones','cpu'),('constant-ones','pio'),('rom-prefix','cpu')):
            for retry in (False,True):
                with tempfile.TemporaryDirectory() as tmp:
                    root=Path(tmp);rom=root/'reference.rom';prc=root/'stub.prc'
                    rom.write_bytes(self.rom);prc.write_bytes(b'PRC');session=root/'session'
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(m.main(['setup',str(session),'--rom',str(rom),'--prc',str(prc)]),0)
                    state=json.loads((session/'session.json').read_text());state.update(installed=True,transport_verified=True,transport='palm-sync',validation=[{'kind':2},{'kind':3},{'kind':2},{'kind':3},{'kind':2}])
                    m.atomic_json(session/'session.json',state)
                    final=dict(run=1,limit=64,requests=64,posted=0 if response=='pio' else 64,completed=64,enabled=64,not_enabled=0,address_errors=0,anomalies=0,stopped=1,arm_pending=0)
                    before=dict(final,run=0,requests=0,posted=0,completed=0,enabled=0,rom_ready=1,ready=1)
                    suffix={'constant-zero':'zero','constant-ones':'ones','rom-prefix':'rom'}[kind]
                    config=dict(response=response+'-'+suffix,marker='reply',words=64)
                    records=[m.fixture_record(state['session'],self.rom,1,m.KIND[kind],0),m.fixture_record(state['session'],self.rom,1,m.KIND[kind],1,m.expected_payload(self.rom,m.KIND[kind]))]
                    armed=dict(before,run=1,limit=64,stopped=0)
                    attempts=[]
                    def transfer(_session,output,mode,names,timeout,**kwargs):
                        self.assertEqual(kwargs['transport'],'palm-sync');attempts.append(1)
                        if retry and len(attempts)==1:raise m.CollectionError('missed HotSync')
                        (output/(m.DB+'.pdb')).write_bytes(m.make_pdb(m.DB,records))
                    with patch.object(m,'RP') as cls,patch.object(m,'rp_port',return_value='/fake'),patch.object(m,'transfer',side_effect=transfer),contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                        rp=cls.return_value;rp.identify_latency.return_value=(before,config);rp.arm.return_value=armed;rp.wait.return_value=final;rp.stop.return_value=final
                        rc=m.main(['run',str(session),'--kind',kind,'--responder',response,'--marker','reply'])
                        self.assertEqual(rc,2 if retry else 0)
                        if retry:
                            self.assertIsNotNone(json.loads((session/'session.json').read_text())['pending'])
                            self.assertEqual(m.main(['collect',str(session)]),0)
                        rp.arm.assert_called_once_with(before,64)
                        rp.identify_latency.assert_called_once_with(self.rom,response+'-'+suffix,'reply')
                        cls.assert_called_once()
                    self.assertIsNone(json.loads((session/'session.json').read_text())['pending'])
                    result=json.loads(next(session.glob('run-*/report.json')).read_text())
                    self.assertEqual(result['rp_config'],config)
                    self.assertEqual(result['result'],'PASS')

class FullLatency(unittest.TestCase):
    setUp=Records.setUp
    pair=Records.pair

    def test_explicit_engine_and_matching_budget_capability(self):
        rp=m.RP.__new__(m.RP)
        before={'stopped':1}
        config=dict(response='cpu-rom',marker='drive',words=64)
        good=f'LATENCY response=cpu-rom marker=drive words=64\r\nENGINE=rx-first-v2 full_words={len(self.rom)//2}\r\n'
        for text,accepted in [(good,True),('',False),('ENGINE=rx-first-v1\r\n',False),
                              (good.replace('full_words=3624','full_words=64'),False),
                              (good.replace('rx-first-v2','rx-first-v20'),False)]:
            rp.identity_text=text
            with patch.object(rp,'identify_latency',return_value=(before,config)),patch.object(rp,'arm') as arm:
                if accepted:
                    state,full=rp.identify_latency_full(self.rom,'drive')
                    self.assertEqual(state,before)
                    self.assertEqual(full,dict(config,words=3624,engine='rx-first-v2',full_image=True))
                else:
                    with self.assertRaises(m.CollectionError):rp.identify_latency_full(self.rom,'drive')
                arm.assert_not_called()

    def test_full_arm_uses_B_and_rejects_short_arm(self):
        rp=m.RP.__new__(m.RP)
        before=dict(run=0,rom_ready=1,ready=1,stopped=1,arm_pending=0)
        armed=dict(run=1,stopped=0,arm_pending=0,limit=3624,requests=0,posted=0,
                   completed=0,enabled=0,not_enabled=0,address_errors=0,anomalies=0)
        with patch.object(rp,'send') as send,patch.object(rp,'status',return_value=armed):
            self.assertEqual(rp.arm(before,3624,command='B'),armed)
            send.assert_called_once_with('B')
        with patch.object(rp,'send'),patch.object(rp,'status',return_value=dict(armed,limit=64)):
            with self.assertRaises(m.CollectionError):rp.arm(before,3624,command='B')

    def test_full_report_requires_full_capability_and_both_budgets(self):
        d=m.decode_record(self.pair(kind=1)[1],self.rom)
        d.update(latency_full_rom=True,rp_armed=dict(run=1,limit=3624))
        good=dict(response='cpu-rom',marker='drive',words=3624,engine='rx-first-v2',full_image=True)
        rp=dict(run=1,limit=3624,requests=3624,posted=3624,completed=3624,enabled=3624,
                stopped=1,arm_pending=0,not_enabled=0,address_errors=0,anomalies=0)
        bad_configs=[None,dict(good,words=64),dict(good,engine='rx-first-v1'),
                     dict(good,response='cpu-ones'),dict(good,full_image=False)]
        with tempfile.TemporaryDirectory() as tmp:
            for config in [good,*bad_configs]:
                if config is None:d.pop('rp_config',None)
                else:d['rp_config']=config
                self.assertEqual(m.report(Path(tmp),d,rp)['result'],'PASS' if config==good else 'COLLECTION ERROR')
            d['rp_config']=good;d['rp_armed']['limit']=64
            self.assertEqual(m.report(Path(tmp),d,rp)['result'],'COLLECTION ERROR')

    def test_full_flag_cannot_arm_patterns_or_PIO(self):
        for args in [('--kind','constant-ones'),('--kind','rom-prefix'),('--responder','pio'),
                     ('--kind','synthetic-pass')]:
            with patch.object(m,'execute') as execute,contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(m.main(['run','/not-a-session','--latency-full-rom',*args]),2)
                execute.assert_not_called()

    def test_full_workflow_and_hotsync_retry_preserve_history(self):
        for retry in (False,True):
            with tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc'
                rom.write_bytes(self.rom);prc.write_bytes(b'PRC');session=root/'session'
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(m.main(['setup',str(session),'--rom',str(rom),'--prc',str(prc)]),0)
                state=json.loads((session/'session.json').read_text());state.update(last_sequence=1,installed=True,transport_verified=True,transport='palm-sync',validation=[{'kind':2},{'kind':3},{'kind':2},{'kind':3},{'kind':2}]);m.atomic_json(session/'session.json',state)
                count=len(self.rom)//2
                final=dict(run=3,limit=count,requests=count,posted=count,completed=count,enabled=count,
                           stopped=1,arm_pending=0,not_enabled=0,address_errors=0,anomalies=0)
                before=dict(final,run=2,requests=0,posted=0,completed=0,enabled=0,rom_ready=1,ready=1)
                config=dict(response='cpu-rom',marker='drive',words=count,engine='rx-first-v2',full_image=True)
                records=[]
                for seq,kind in [(1,6),(2,1)]:
                    records.extend([m.fixture_record(state['session'],self.rom,seq,kind,0),
                                    m.fixture_record(state['session'],self.rom,seq,kind,1,m.expected_payload(self.rom,kind))])
                attempts=[]
                def transfer(_session,output,mode,names,timeout,**kwargs):
                    self.assertEqual(kwargs['transport'],'palm-sync');attempts.append(1)
                    if retry and len(attempts)==1:raise m.CollectionError('missed HotSync')
                    (output/(m.DB+'.pdb')).write_bytes(m.make_pdb(m.DB,records))
                with patch.object(m,'RP') as cls,patch.object(m,'rp_port',return_value='/fake'),patch.object(m,'transfer',side_effect=transfer),contextlib.redirect_stdout(io.StringIO()),contextlib.redirect_stderr(io.StringIO()):
                    rp=cls.return_value;rp.identify_latency_full.return_value=(before,config)
                    rp.arm.return_value=dict(before,run=3,stopped=0);rp.wait.return_value=final;rp.stop.return_value=final
                    self.assertEqual(m.main(['run',str(session),'--latency-full-rom']),2 if retry else 0)
                    if retry:
                        pending=json.loads((session/'session.json').read_text())['pending']
                        self.assertTrue(pending['latency_full_rom'])
                        self.assertEqual(m.main(['collect',str(session)]),0)
                    rp.arm.assert_called_once_with(before,count,command='B')
                    rp.identify_latency_full.assert_called_once_with(self.rom,'drive')
                    rp.identify.assert_not_called();rp.identify_latency.assert_not_called()
                updated=json.loads((session/'session.json').read_text())
                self.assertEqual(updated['last_sequence'],2);self.assertIsNone(updated['pending'])
                result=json.loads(next(session.glob('run-*/report.json')).read_text())
                self.assertTrue(result['latency_full_rom']);self.assertEqual(result['rp_config'],config)
                self.assertEqual(result['result'],'PASS')

    def test_unsupported_full_engine_does_not_reserve_or_arm(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc';rom.write_bytes(self.rom);prc.write_bytes(b'PRC');session=root/'session'
            with contextlib.redirect_stdout(io.StringIO()):m.main(['setup',str(session),'--rom',str(rom),'--prc',str(prc)])
            state=json.loads((session/'session.json').read_text());state.update(transport_verified=True,validation=[{'kind':2},{'kind':3},{'kind':2},{'kind':3},{'kind':2}]);m.atomic_json(session/'session.json',state)
            with patch.object(m,'RP') as cls,patch.object(m,'rp_port',return_value='/fake'),contextlib.redirect_stderr(io.StringIO()):
                rp=cls.return_value;rp.identify_latency_full.side_effect=m.CollectionError('unsupported engine')
                rp.stop.return_value=dict(stopped=1,arm_pending=0)
                self.assertEqual(m.main(['run',str(session),'--latency-full-rom']),2)
                rp.arm.assert_not_called()
            state=json.loads((session/'session.json').read_text());self.assertEqual(state['last_sequence'],0);self.assertIsNone(state['pending'])

    def test_prior_checker_patterns_do_not_qualify_optimized_full_read(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);rom=root/'input.rom';prc=root/'input.prc'
            rom.write_bytes(self.rom);prc.write_bytes(b'PRC');session=root/'session'
            with contextlib.redirect_stdout(io.StringIO()):
                m.main(['setup',str(session),'--rom',str(rom),'--prc',str(prc)])
            state=json.loads((session/'session.json').read_text())
            state.update(transport_verified=True,last_app_version=0x601,
                validation=[{'kind':2},{'kind':3},{'kind':2},{'kind':3},{'kind':2}],
                spacing_validation=[dict(kind=k,read_spacing='none',capture_profile='burst64',
                    words=64,app_version=0x600) for k in (4,5)])
            m.atomic_json(session/'session.json',state)
            with patch.object(m,'RP') as cls,contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(m.main(['run',str(session),'--latency-full-rom',
                    '--read-spacing','none','--capture-profile','burst64']),2)
                cls.assert_not_called()
            after=json.loads((session/'session.json').read_text())
            self.assertEqual(after,state)

if __name__=='__main__':unittest.main()

class ContinuousEvidence(unittest.TestCase):
    def fixture(self):
        rom=bytes(range(256))*28+bytes(range(80))
        session='0123456789abcdef'*2
        records=[m.fixture_record(session,rom,1,1,0),m.fixture_record(session,rom,1,1,1,rom)]
        d=m.decode_pdb(m.make_pdb(m.DB,records),session,rom)[1]
        n=len(rom)//2
        rp=dict(run=7,limit=0,continuous=1,stopped=1,arm_pending=0,
                requests=n,posted=n,completed=n,attempted=n,cancelled=0,empty_ends=0)
        d['rp_armed']=dict(rp,stopped=0)
        d['rp_config']=dict(engine='continuous-v1',continuous=True,words=n,
                            response='cpu-rom',marker='drive',full_image=True)
        return d,rp

    def test_continuous_complete_and_failed_ownership(self):
        d,rp=self.fixture()
        with tempfile.TemporaryDirectory() as tmp:
            m.report(Path(tmp),d,rp)
            self.assertEqual(json.loads((Path(tmp)/'report.json').read_text())['result'],'PASS')
        for field,value in [('limit',3624),('continuous',0),('run',8),('stopped',0),
                            ('requests',1),('posted',1),('attempted',1),('cancelled',1),('empty_ends',1)]:
            with self.subTest(field=field),tempfile.TemporaryDirectory() as tmp:
                m.report(Path(tmp),d,dict(rp,**{field:value}))
                self.assertEqual(json.loads((Path(tmp)/'report.json').read_text())['result'],'COLLECTION ERROR')

    def test_wrong_mode_cannot_qualify_rom(self):
        d,rp=self.fixture();d['rp_config']['response']='cpu-zero'
        with tempfile.TemporaryDirectory() as tmp:
            m.report(Path(tmp),d,rp)
            self.assertEqual(json.loads((Path(tmp)/'report.json').read_text())['result'],'COLLECTION ERROR')

class CheckerProfiles(unittest.TestCase):
    def tearDown(self):
        m.configure_profile('baseline')

    def test_distinct_creator_and_database_reject_cross_profile(self):
        rom=bytes(range(256))*2;session='0123456789abcdef'*2
        m.configure_profile('baseline')
        baseline=m.make_pdb(m.DB,[m.fixture_record(session,rom,1,2,0)])
        m.configure_profile('romprobe')
        self.assertEqual(m.DB_CREATOR,b'FNVD')
        self.assertEqual(m.CONTROL_DB,'ROMProbeControl')
        alternate=m.make_pdb(m.DB,[m.fixture_record(session,rom,1,2,0)])
        self.assertEqual(m.decode_pdb(alternate,session,rom)[1]['result'],'INCOMPLETE')
        with self.assertRaises(m.CollectionError):m.decode_pdb(baseline,session,rom)
        m.configure_profile('baseline')
        with self.assertRaises(m.CollectionError):m.decode_pdb(alternate,session,rom)

    def test_cli_profile_setup_and_baseline_session_rejection(self):
        rom=bytes(range(256))*2
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'rom').write_bytes(rom)
            prc=bytearray(78);prc[60:68]=b'applFNVD';(root/'prc').write_bytes(prc)
            with contextlib.redirect_stdout(io.StringIO()):
                rc=m.main(['setup',str(root/'session'),'--rom',str(root/'rom'),
                           '--prc',str(root/'prc'),'--profile','romprobe'])
            self.assertEqual(rc,0)
            self.assertEqual(json.loads((root/'session/session.json').read_text())['profile'],'romprobe')
            control=(root/'session/ROMProbeControl.pdb').read_bytes()
            self.assertEqual(control[64:68],b'FNVD')
            m.configure_profile('baseline')
            with self.assertRaisesRegex(m.CollectionError,'profile mismatch'):
                m.load_session(root/'session')

class ContinuousConsole(unittest.TestCase):
    def test_status_refuses_bounded_or_inactive_arm(self):
        rp=m.ContinuousRP.__new__(m.ContinuousRP)
        fields=dict(rom_ready=1,ready=1,stopped=0,arm_pending=0,active=1,run=2,
                    requests=0,posted=0,completed=0,limit=0,attempted=0,cancelled=0,empty_ends=0,continuous=1)
        rp.send=lambda _:None
        rp.read=lambda _: ' '.join(f'{k}={v}' for k,v in fields.items())
        self.assertEqual(rp.status()['limit'],0)
        fields['limit']=64
        with self.assertRaises(m.CollectionError):rp.status()
        fields['limit']=0;fields['active']=0
        with self.assertRaisesRegex(m.CollectionError,'arm confirmation'):
            rp.arm(dict(rom_ready=1,ready=1,stopped=1,arm_pending=0,run=1),64)

    def test_identity_requires_exact_image_and_new_banner(self):
        rp=m.ContinuousRP.__new__(m.ContinuousRP)
        rp.stop=lambda: {'stopped':1}
        rp.send=lambda _:None
        rom=bytes(range(256))*2
        text=f'CONTINUOUS version=1 response=cpu-rom marker=drive continuous=1 words=256 crc={m.crc(rom):08x} clock_khz=150000'
        rp.read=lambda _:text
        self.assertEqual(rp.identify_continuous(rom,'cpu-rom',64)[1]['engine'],'continuous-v1')
        text=text.replace('version=1','version=2')
        rp.stop=lambda: {'stopped':1,'counters_final':1}
        self.assertEqual(rp.identify_continuous(rom,'cpu-rom',64)[1]['engine'],'continuous-v2')
        text=text.replace('marker=drive','marker=preload')
        with self.assertRaises(m.CollectionError):rp.identify_continuous(rom,'cpu-rom',64)
        self.assertEqual(rp.identify_continuous(rom,'cpu-rom',64,marker='preload')[1]['marker'],'preload')
        text=text.replace('marker=preload','marker=drive')
        text=text.replace('version=2','version=3')
        with self.assertRaises(m.CollectionError):rp.identify_continuous(rom,'cpu-rom',64)
        with self.assertRaises(m.CollectionError):rp.identify_continuous(rom+b'00','cpu-rom',64)
        text='FujiSpring LATENCY DIAGNOSTIC / bounded reads / 150MHz'
        with self.assertRaises(m.CollectionError):rp.identify_continuous(rom,'cpu-rom',64)

    def test_multiline_status_complete_and_truncated_last_page(self):
        rp=m.ContinuousRP.__new__(m.ContinuousRP);rp.send=lambda _:None
        pages=['rom_ready=1 ready=1 stopped=1 arm_pending=0 active=0 run=0 limit=0 continuous=1\r\n',
               'requests=0 posted=0 completed=0 attempted=0\r\n',
               'cancelled=0 empty_ends=0\r\n']
        rp.read=lambda _:''.join(pages)
        self.assertEqual(rp.status()['empty_ends'],0)
        pages.pop()
        with self.assertRaisesRegex(m.CollectionError,'incomplete'):
            rp.status()
