import hid

VID = 0x1001
PID = 0xF103  # HelloWord-Keyboard-fw
# PID = 0x03EF  # HelloWord-Dynamic-fw
RAW_HID_USAGE_PAGE = 0xFFC0

devices = hid.enumerate(VID, PID)
if not devices:
    print(f"No device found with VID=0x{VID:04X} PID=0x{PID:04X}")
    print("\nAll HID devices:")
    for d in hid.enumerate():
        print(f"  VID=0x{d['vendor_id']:04X} PID=0x{d['product_id']:04X} "
              f"usage_page=0x{d['usage_page']:04X} usage=0x{d['usage']:04X} "
              f"- {d['product_string']}")
    exit(1)

print(f"Found {len(devices)} interface(s):")
raw_hid_path = None
for d in devices:
    tag = ""
    if d['usage_page'] == RAW_HID_USAGE_PAGE:
        raw_hid_path = d['path']
        tag = " <-- Raw HID"
    print(f"  usage_page=0x{d['usage_page']:04X} usage=0x{d['usage']:04X} "
          f"interface={d['interface_number']}{tag}")

if not raw_hid_path:
    print("Error: Raw HID interface (usage_page=0xFFC0) not found!")
    exit(1)

dev = hid.device()
dev.open_path(raw_hid_path)
report = [0x02, 0xDF] + [0x00] * 31
dev.write(report)
dev.close()
print("DFU command sent, keyboard should reboot to bootloader")
