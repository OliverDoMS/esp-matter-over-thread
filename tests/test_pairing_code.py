import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import pairing_code  # noqa: E402


class PairingCodeTest(unittest.TestCase):
    def test_manual_code_matches_matter_test_vector(self):
        # Test credentials of the Matter SDK: passcode 20202021, discriminator 3840.
        self.assertEqual(pairing_code.manual_code(20202021, 3840), "34970112332")

    def test_derivation_is_stable_and_valid(self):
        mac = bytes.fromhex("744dbd65e14d")
        passcode, discriminator = pairing_code.derive(mac)
        self.assertEqual((passcode, discriminator), pairing_code.derive(mac))
        self.assertTrue(pairing_code.passcode_valid(passcode))
        self.assertLess(discriminator, 4096)

    def test_devices_differ(self):
        a = pairing_code.derive(bytes.fromhex("744dbd65e14d"))
        b = pairing_code.derive(bytes.fromhex("744dbd65e14e"))
        self.assertNotEqual(a, b)

    def test_forbidden_passcodes_rejected(self):
        for bad in (0, 11111111, 12345678, 87654321, 99999999, 100000000):
            self.assertFalse(pairing_code.passcode_valid(bad))

    def test_mac_parsing(self):
        self.assertEqual(pairing_code.parse_mac("74:4d:bd:65:e1:4d"), bytes.fromhex("744dbd65e14d"))
        with self.assertRaises(ValueError):
            pairing_code.parse_mac("74:4d")


if __name__ == "__main__":
    unittest.main()
