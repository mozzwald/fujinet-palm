import contextlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock,patch
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import continuous_rom as service
import prism_collect as p


class RecognitionService(unittest.TestCase):
    def test_persistent_gate_fails_before_opening_responder(self):
        with tempfile.TemporaryDirectory() as tmp:
            rom=Path(tmp)/'rom';rom.write_bytes(b'rom!')
            argv=['continuous_rom.py','--rom',str(rom),'--log-dir',str(Path(tmp)/'log'),'--engine','persistent','--marker','preload']
            with patch('sys.argv',argv),patch.object(p,'ContinuousRP') as ctor,contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(service.main(),2)
            ctor.assert_not_called()

    def test_persistent_service_stays_quiet_and_does_not_stop_for_hotsync(self):
        with tempfile.TemporaryDirectory() as tmp:
            rom=Path(tmp)/'rom';rom.write_bytes(b'rom!')
            argv=['continuous_rom.py','--rom',str(rom),'--log-dir',str(Path(tmp)/'log'),'--engine','persistent','--marker','preload',
                  '--readback-session',str(Path(tmp)/'readback'),'--firmware-sha256','11'*32]
            rp=Mock();rp.identify_continuous.return_value=({'stopped':1},{'engine':'continuous-v3'});rp.arm.return_value={'run':1};rp.stop.return_value={'stopped':1}
            with patch('sys.argv',argv),patch.object(p,'ContinuousRP',return_value=rp),patch.object(p,'rp_port',return_value='/dev/ttyACM0'),\
                 patch('qualified_rom_readback.readback_gate',return_value={'verified':True}),\
                 patch.object(service.time,'sleep',side_effect=[None,KeyboardInterrupt]),\
                 patch.object(p,'find_palm',side_effect=AssertionError('must not stop on HotSync')),contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(service.main(),0)
            rp.identify_continuous.assert_called_once_with(b'rom!','cpu-rom',2,marker='preload',engine='persistent')
            rp.status.assert_not_called()
            rp.stop.assert_called_once()  # Explicit Ctrl-C in this test, after quiet iteration.
