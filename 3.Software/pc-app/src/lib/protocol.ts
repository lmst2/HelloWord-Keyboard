// Mirrors firmware protocol.h — message command IDs

export const Msg = {
  // PC -> Keyboard
  PC_KB_RGB_MODE: 0x01,
  PC_KB_RGB_DIRECT: 0x02,
  PC_KB_CONFIG_GET: 0x05,
  PC_KB_CONFIG_SET: 0x06,
  PC_KB_STATUS_REQ: 0x07,
  PC_KB_CONFIG_GET_ALL: 0x08,
  PC_KB_DFU: 0xdf,

  // KB -> PC
  KB_PC_STATUS: 0x81,
  KB_PC_CONFIG_VALUE: 0x82,
  KB_PC_ACK: 0x83,

  // PC -> Hub
  PC_HUB_CONFIG_GET: 0xc1,
  PC_HUB_CONFIG_SET: 0xc2,
  PC_HUB_DATA_FEED: 0xc5,
  PC_HUB_EINK_IMAGE: 0xc6,
  PC_HUB_EINK_TEXT: 0xc7,
  PC_HUB_APP_SWITCH: 0xd1,
  PC_HUB_EINK_SWITCH: 0xd2,

  // Hub -> PC
  HUB_PC_CONFIG_VALUE: 0xe1,
  HUB_PC_STATUS: 0xe2,
  HUB_PC_ACK: 0xe3,
  HUB_PC_FW_INFO: 0xe4,
} as const;

export const FEED_CPU_USAGE = 0x01;
export const FEED_RAM_USAGE = 0x02;
export const FEED_CPU_TEMP = 0x03;
export const FEED_DISK_USAGE = 0x04;
export const FEED_GPU_USAGE = 0x10;
export const FEED_GPU_TEMP = 0x11;
