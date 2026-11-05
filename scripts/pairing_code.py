"""Print the Matter pairing data of one device from its base MAC address.

The firmware (main/device_identity.cpp) derives passcode and discriminator from the
chip's unique base MAC; this script repeats the derivation, so the manual pairing code
can be printed on a label before the device is ever connected.

    python scripts/pairing_code.py 74:4d:bd:65:e1:4d
"""
import hashlib
import sys

CONTEXT = b"DoNetIoT-rain-v1"  # Must match main/device_identity.cpp
INVALID_PASSCODES = {11111111, 22222222, 33333333, 44444444, 55555555,
                     66666666, 77777777, 88888888, 99999999, 12345678, 87654321}

_VERHOEFF_D = [
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 2, 3, 4, 0, 6, 7, 8, 9, 5],
    [2, 3, 4, 0, 1, 7, 8, 9, 5, 6], [3, 4, 0, 1, 2, 8, 9, 5, 6, 7],
    [4, 0, 1, 2, 3, 9, 5, 6, 7, 8], [5, 9, 8, 7, 6, 0, 4, 3, 2, 1],
    [6, 5, 9, 8, 7, 1, 0, 4, 3, 2], [7, 6, 5, 9, 8, 2, 1, 0, 4, 3],
    [8, 7, 6, 5, 9, 3, 2, 1, 0, 4], [9, 8, 7, 6, 5, 4, 3, 2, 1, 0],
]
_VERHOEFF_P = [
    [0, 1, 2, 3, 4, 5, 6, 7, 8, 9], [1, 5, 7, 6, 2, 8, 3, 0, 9, 4],
    [5, 8, 0, 3, 7, 9, 6, 1, 4, 2], [8, 9, 1, 6, 0, 4, 3, 5, 2, 7],
    [9, 4, 5, 3, 1, 2, 6, 8, 7, 0], [4, 2, 8, 6, 5, 7, 3, 9, 0, 1],
    [2, 7, 9, 3, 8, 0, 6, 4, 1, 5], [7, 0, 4, 6, 9, 1, 3, 2, 5, 8],
]
_VERHOEFF_INV = [0, 4, 3, 2, 1, 5, 6, 7, 8, 9]


def _check_digit(digits: str) -> str:
    c = 0
    for i, ch in enumerate(reversed(digits)):
        c = _VERHOEFF_D[c][_VERHOEFF_P[(i + 1) % 8][int(ch)]]
    return str(_VERHOEFF_INV[c])


def passcode_valid(passcode: int) -> bool:
    return 1 <= passcode <= 99999998 and passcode not in INVALID_PASSCODES


def derive(mac: bytes):
    """Returns (passcode, discriminator) for a 6-byte base MAC."""
    h = hashlib.sha256(CONTEXT + mac).digest()
    while True:
        passcode = int.from_bytes(h[0:4], "big") % 99999998 + 1
        if passcode_valid(passcode):
            break
        h = hashlib.sha256(h).digest()
    return passcode, int.from_bytes(h[4:6], "big") & 0x0FFF


def manual_code(passcode: int, discriminator: int) -> str:
    """11-digit manual pairing code (short form, without vendor/product ID)."""
    chunk1 = discriminator >> 10
    chunk2 = ((discriminator & 0x300) << 6) | (passcode & 0x3FFF)
    chunk3 = passcode >> 14
    digits = f"{chunk1}{chunk2:05d}{chunk3:04d}"
    return digits + _check_digit(digits)


def parse_mac(text: str) -> bytes:
    mac = bytes.fromhex(text.replace(":", "").replace("-", ""))
    if len(mac) != 6:
        raise ValueError("MAC must have 6 bytes")
    return mac


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    mac = parse_mac(sys.argv[1])
    passcode, discriminator = derive(mac)
    print(f"Serial:        DNIOT-{mac.hex().upper()}")
    print(f"Passcode:      {passcode:08d}")
    print(f"Discriminator: {discriminator}")
    print(f"Manual code:   {manual_code(passcode, discriminator)}")


if __name__ == "__main__":
    main()
