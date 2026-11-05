"""Look for Matter devices in commissioning mode (BLE service 0xFFF6); needs bleak.

    python scripts/ble_scan.py [seconds]
"""
import asyncio
import sys

from bleak import BleakScanner


async def main(seconds: float) -> None:
    seen = {}

    def on_advertisement(device, advertisement):
        seen[device.address] = (device.name, advertisement.rssi, dict(advertisement.service_data))

    async with BleakScanner(on_advertisement):
        await asyncio.sleep(seconds)

    found = False
    for address, (name, rssi, service_data) in seen.items():
        for uuid, data in service_data.items():
            if "fff6" in uuid.lower() and len(data) >= 7:
                found = True
                # opcode (1) | discriminator and version (2) | vendor ID (2) | product ID (2)
                discriminator = (data[1] | (data[2] << 8)) & 0x0FFF
                vendor = data[3] | (data[4] << 8)
                product = data[5] | (data[6] << 8)
                print(f"MATTER {address} {name} rssi={rssi} discriminator={discriminator} "
                      f"vendor=0x{vendor:04X} product=0x{product:04X}")
    print(f"Matter device found: {found} ({len(seen)} BLE devices seen)")


if __name__ == "__main__":
    asyncio.run(main(float(sys.argv[1]) if len(sys.argv) > 1 else 25))
