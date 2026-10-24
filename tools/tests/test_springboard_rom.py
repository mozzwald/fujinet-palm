"""Validate real build artifacts and reject independently mutated card images."""
import binascii
import importlib.util
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('rom_verify', ROOT/'tools/verify-springboard-rom.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class ROMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        app = ROOT/'palm/apps/visorbus'
        cls.rom = (app/'build/visorbus.rom').read_bytes()
        cls.prc = (app/'visorbus.prc').read_bytes()

    def recalc(self, data):
        count = struct.unpack_from('>I', data, 108)[0]
        crc = binascii.crc_hqx(data[:112], 0)
        crc = binascii.crc_hqx(data[114:count], crc)
        struct.pack_into('>H', data, 112, crc)

    def test_good(self):
        result = module.verify(self.rom, self.prc)
        self.assertEqual(result['access_time_ns'], 300)
        self.assertGreaterEqual(result['resource_count'], 7)
        self.assertFalse(result['hardware_verified'])

    def test_crc(self):
        data = bytearray(self.rom); data[400] ^= 1
        with self.assertRaisesRegex(ValueError, 'CRC16'):
            module.verify(data, self.prc)

    def test_signature(self):
        data = bytearray(self.rom); data[8] ^= 1
        with self.assertRaisesRegex(ValueError, 'signature'):
            module.verify(data, self.prc)

    def test_token_with_valid_crc(self):
        data = bytearray(self.rom)
        token = struct.unpack_from('>I', data, 100)[0] - 0x08000000
        data[token+6] = ord('2'); self.recalc(data)
        with self.assertRaisesRegex(ValueError, '300 ns'):
            module.verify(data, self.prc)

    def test_bad_resource_pointer_with_valid_crc(self):
        data = bytearray(self.rom)
        directory = struct.unpack_from('>I', data, 316)[0] - 0x08000000
        db = struct.unpack_from('>I', data, directory+6)[0] - 0x08000000
        struct.pack_into('>I', data, db+84, 0x09000000); self.recalc(data)
        with self.assertRaisesRegex(ValueError, 'local ID'):
            module.verify(data, self.prc)

    def test_resource_payload_with_valid_crc(self):
        data = bytearray(self.rom)
        directory = struct.unpack_from('>I', data, 316)[0] - 0x08000000
        db = struct.unpack_from('>I', data, directory+6)[0] - 0x08000000
        payload = struct.unpack_from('>I', data, db+84)[0] - 0x08000000
        data[payload] ^= 1; self.recalc(data)
        with self.assertRaisesRegex(ValueError, 'resource differs'):
            module.verify(data, self.prc)

    def test_truncated(self):
        with self.assertRaises(ValueError):
            module.verify(self.rom[:-2], self.prc)

class DistinctROMTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        app=ROOT/'palm/apps/romprobe'
        cls.rom=(app/'build/romprobe.rom').read_bytes()
        cls.prc=(app/'romprobe.prc').read_bytes()

    def test_explicit_distinct_identity(self):
        result=module.verify(self.rom,self.prc,b'FNVR')
        self.assertEqual(result['database'],'FujiNet ROM Probe')
        self.assertEqual(result['creator'],'FNVR')
        self.assertEqual(result['access_time_ns'],330)
        self.assertFalse(result['hardware_verified'])
        with self.assertRaises(ValueError):
            module.verify(self.rom,self.prc)
        with self.assertRaises(ValueError):
            module.verify((ROOT/'palm/apps/visorbus/build/visorbus.rom').read_bytes(),
                          (ROOT/'palm/apps/visorbus/visorbus.prc').read_bytes(),b'FNVR')

    def recalc(self,data):
        crc=binascii.crc_hqx(data[:112],0)
        crc=binascii.crc_hqx(data[114:],crc)
        struct.pack_into('>H',data,112,crc)

    def test_no_reset_after_install_notification(self):
        data=bytearray(self.rom)
        directory=struct.unpack_from('>I',data,316)[0]-0x08000000
        db=struct.unpack_from('>I',data,directory+6)[0]-0x08000000
        attr=struct.unpack_from('>H',data,db+32)[0]
        struct.pack_into('>H',data,db+32,attr|0x0020);self.recalc(data)
        with self.assertRaisesRegex(ValueError,'reset notification'):
            module.verify(data,self.prc,b'FNVR')

    def test_probe_requires_current_330ns_token(self):
        data=bytearray(self.rom)
        token=struct.unpack_from('>I',data,100)[0]-0x08000000
        data[token+6:token+9]=b'300';self.recalc(data)
        with self.assertRaisesRegex(ValueError,'330 ns'):
            module.verify(data,self.prc,b'FNVR')

if __name__ == '__main__':
    unittest.main()
