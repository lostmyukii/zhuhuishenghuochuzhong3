import tempfile
import unittest
from datetime import datetime
from pathlib import Path
from zoneinfo import ZoneInfo

from g2_readonly_preflight import EXPECTED_PINS, build_report, enumerate_ports


class G2ReadonlyPreflightTests(unittest.TestCase):
    def test_report_is_truthful_and_never_authorizes_upload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            config = root / "firmware/include/smartlife_config.h"
            binary = root / "firmware/.pio/build/n16r8_esp32s3/firmware.bin"
            devices = root / "dev"
            config.parent.mkdir(parents=True)
            binary.parent.mkdir(parents=True)
            devices.mkdir()
            config.write_text(
                "\n".join(
                    f"constexpr uint8_t {name} = {value};"
                    for name, value in EXPECTED_PINS.items()
                ),
                encoding="utf-8",
            )
            binary.write_bytes(b"n16r8-test-firmware")
            (devices / "cu.usbserial-TEST").touch()
            report = build_report(
                root,
                device_directory=devices,
                now=datetime(2026, 7, 20, tzinfo=ZoneInfo("Asia/Shanghai")),
            )

            self.assertEqual("hardware-preflight-pending", report["status"])
            self.assertFalse(report["uploadAuthorized"])
            self.assertFalse(report["uploadPerformed"])
            self.assertTrue(report["serialPort"]["confirmed"])
            self.assertTrue(report["gpioContract"]["allMatch"])
            self.assertTrue(all(item["status"] == "pending" for item in report["manualChecks"]))

    def test_port_enumeration_ignores_unrelated_serial_devices(self):
        with tempfile.TemporaryDirectory() as directory:
            devices = Path(directory)
            (devices / "cu.Bluetooth-Incoming-Port").touch()
            (devices / "cu.usbmodem2101").touch()
            self.assertEqual([str(devices / "cu.usbmodem2101")], enumerate_ports(devices))


if __name__ == "__main__":
    unittest.main()
