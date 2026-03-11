#!/usr/bin/env python3
"""Flash firmware to HW75 keyboard via USB HID bootloader.

Usage:
    python flash_firmware.py <firmware.bin>

Requirements:
    pip install hidapi
"""

import sys
import struct
import time
import binascii

try:
    import hid
except ImportError:
    print("[ERROR] 'hidapi' package required: pip install hidapi")
    sys.exit(1)

VID = 0x1001
PID = 0xB007
USAGE_PAGE = 0xFF00

CMD_INFO   = 0x01
CMD_ERASE  = 0x02
CMD_WRITE  = 0x03
CMD_SEAL   = 0x04
CMD_REBOOT = 0x05

RSP_OK        = 0x00
RSP_ERR       = 0x01
RSP_BAD_STATE = 0x02
RSP_BAD_ADDR  = 0x03
RSP_CRC_FAIL  = 0x04

WRITE_CHUNK = 60

STATUS_NAMES = {
    RSP_OK: "OK",
    RSP_ERR: "error",
    RSP_BAD_STATE: "bad state (erase first?)",
    RSP_BAD_ADDR: "bad address",
    RSP_CRC_FAIL: "CRC mismatch",
}


def open_device():
    devs = hid.enumerate(VID, PID)
    path = None
    for d in devs:
        if d["usage_page"] == USAGE_PAGE:
            path = d["path"]
            break
    if not path and devs:
        path = devs[0]["path"]
    if not path:
        return None
    dev = hid.device()
    dev.open_path(path)
    return dev


def send(dev, payload, timeout_ms=5000):
    buf = bytes([0x00]) + bytes(payload) + b"\x00" * (64 - len(payload))
    dev.write(buf[:65])
    resp = dev.read(64, timeout_ms=timeout_ms)
    if not resp or len(resp) < 2:
        raise RuntimeError("No response from device")
    return bytes(resp)


def check(resp, cmd, context=""):
    if resp[0] != cmd:
        raise RuntimeError(f"Unexpected response 0x{resp[0]:02X} for cmd 0x{cmd:02X}")
    if resp[1] != RSP_OK:
        status = STATUS_NAMES.get(resp[1], f"0x{resp[1]:02X}")
        raise RuntimeError(f"{context}failed: {status}")


def get_info(dev):
    resp = send(dev, [CMD_INFO])
    if resp[0] != CMD_INFO:
        raise RuntimeError("Bad info response")
    app_size = struct.unpack_from("<I", resp, 1)[0]
    page_size = struct.unpack_from("<H", resp, 5)[0]
    ver_major, ver_minor = resp[7], resp[8]
    return app_size, page_size, ver_major, ver_minor


def erase(dev):
    resp = send(dev, [CMD_ERASE], timeout_ms=30000)
    check(resp, CMD_ERASE, "Erase ")


def write_chunk(dev, offset, data):
    pkt = bytes([CMD_WRITE,
                 offset & 0xFF,
                 (offset >> 8) & 0xFF,
                 (offset >> 16) & 0xFF]) + data
    resp = send(dev, pkt)
    check(resp, CMD_WRITE, f"Write @0x{offset:06X} ")


def seal(dev, size, crc):
    pkt = struct.pack("<BII", CMD_SEAL, size, crc)
    resp = send(dev, pkt, timeout_ms=10000)
    check(resp, CMD_SEAL, "Verify ")


def reboot(dev):
    try:
        send(dev, [CMD_REBOOT], timeout_ms=2000)
    except Exception:
        pass


def progress_bar(current, total, width=40):
    pct = min(100, current * 100 // total)
    filled = pct * width // 100
    bar = "#" * filled + "-" * (width - filled)
    print(f"\r  [{bar}] {pct}% ({current}/{total})", end="", flush=True)


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <firmware.bin>")
        sys.exit(1)

    bin_path = sys.argv[1]
    with open(bin_path, "rb") as f:
        firmware = f.read()

    if not firmware:
        print("[ERROR] Firmware file is empty")
        sys.exit(1)

    print(f"[INFO] Firmware: {bin_path} ({len(firmware)} bytes)")

    print("[INFO] Searching for HW75 bootloader...")
    dev = open_device()
    if not dev:
        print(f"[ERROR] Device not found (VID=0x{VID:04X} PID=0x{PID:04X})")
        print("\nAvailable HID devices:")
        for d in hid.enumerate():
            print(f"  VID=0x{d['vendor_id']:04X} PID=0x{d['product_id']:04X} "
                  f"usage=0x{d['usage_page']:04X}:0x{d['usage']:04X} "
                  f"- {d['product_string']}")
        sys.exit(1)

    try:
        app_size, page_size, ver_major, ver_minor = get_info(dev)
        print(f"[INFO] Connected: HW75 Bootloader v{ver_major}.{ver_minor}")
        print(f"[INFO] App region: {app_size // 1024}KB, page: {page_size}B")

        if len(firmware) > app_size:
            print(f"[ERROR] Firmware too large ({len(firmware)} > {app_size} bytes)")
            sys.exit(1)

        print("[INFO] Erasing flash...", end="", flush=True)
        t0 = time.time()
        erase(dev)
        print(f" done ({time.time() - t0:.1f}s)")

        total = len(firmware)
        print(f"[INFO] Writing {total} bytes...")
        t0 = time.time()
        offset = 0
        while offset < total:
            chunk = firmware[offset:offset + WRITE_CHUNK]
            if len(chunk) < WRITE_CHUNK:
                chunk += b"\xFF" * (WRITE_CHUNK - len(chunk))
            write_chunk(dev, offset, chunk)
            offset += WRITE_CHUNK
            progress_bar(min(offset, total), total)
        print(f"\n[INFO] Write done ({time.time() - t0:.1f}s)")

        print("[INFO] Verifying CRC32...", end="", flush=True)
        fw_crc = binascii.crc32(firmware) & 0xFFFFFFFF
        seal(dev, total, fw_crc)
        print(" OK")

        print("[INFO] Rebooting to application...")
        reboot(dev)
        print("[DONE] Firmware flashed successfully!")

    except RuntimeError as e:
        print(f"\n[ERROR] {e}")
        sys.exit(1)
    finally:
        dev.close()


if __name__ == "__main__":
    main()
