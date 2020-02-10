/*
  xdrv_23_zigbee_converters.ino - zigbee support for Tasmota

  Copyright (C) 2020  Theo Arends and Stephan Hadinger

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_ZIGBEE

typedef struct Z_CommandConverter {
  const char * tasmota_cmd;
  uint16_t     cluster;
  uint16_t     cmd;       // normally 8 bits, 0xFFFF means it's a parameter
  const char * param;
} Z_CommandConverter;

typedef struct Z_XYZ_Var {    // Holds values for vairables X, Y and Z
  uint32_t    x = 0;
  uint32_t    y = 0;
  uint32_t    z = 0;
  uint8_t     x_type = 0;     // 0 = no value, 1 = 1 bytes, 2 = 2 bytes
  uint8_t     y_type = 0;
  uint8_t     z_type = 0;
} Z_XYZ_Var;

// list of post-processing directives
const Z_CommandConverter Z_Commands[] = {
  { "Power",          0x0006, 0xFFFF, "" },             // 0=Off, 1=On, 2=Toggle
  { "Dimmer",         0x0008, 0x04,   "xx0A00" },       // Move to Level with On/Off, xx=0..254 (255 is invalid)
  { "Dimmer+",        0x0008, 0x06,   "001902" },       // Step up by 10%, 0.2 secs
  { "Dimmer-",        0x0008, 0x06,   "011902" },       // Step down by 10%, 0.2 secs
  { "DimmerStop",     0x0008, 0x03,   "" },             // Stop any Dimmer animation
  { "ResetAlarm",     0x0009, 0x00,   "xxyyyy" },       // Reset alarm (alarm code + cluster identifier)
  { "ResetAllAlarms", 0x0009, 0x01,   "" },             // Reset all alarms
  { "Hue",            0x0300, 0x00,   "xx000A00" },     // Move to Hue, shortest time, 1s
  { "Sat",            0x0300, 0x03,   "xx0A00" },       // Move to Sat
  { "HueSat",         0x0300, 0x06,   "xxyy0A00" },     // Hue, Sat
  { "Color",          0x0300, 0x07,   "xxxxyyyy0A00" }, // x, y (uint16)
  { "CT",             0x0300, 0x0A,   "xxxx0A00" },     // Color Temperature Mireds (uint16)
  { "ShutterOpen",    0x0102, 0x00,   "" },
  { "ShutterClose",   0x0102, 0x01,   "" },
  { "ShutterStop",    0x0102, 0x02,   "" },
  { "ShutterLift",    0x0102, 0x05,   "xx" },            // Lift percentage, 0%=open, 100%=closed
  { "ShutterTilt",    0x0102, 0x08,   "xx" },            // Tilt percentage
  { "Shutter",        0x0102, 0xFFFF, "" },
  // Blitzwolf PIR
  { "",               0xEF00, 0x01,   ""},                // Specific decoder for Blitzwolf PIR, empty name means special treatment
  // Decoders only - normally not used to send, and names may be masked by previous definitions
  { "Dimmer",         0x0008, 0x00,   "xx" },
  { "DimmerMove",     0x0008, 0x01,   "xx0A" },
  { "DimmerStep",     0x0008, 0x02,   "xx190A00" },
  { "DimmerMove",     0x0008, 0x05,   "xx0A" },
  { "Dimmer+",        0x0008, 0x06,   "00" },
  { "Dimmer-",        0x0008, 0x06,   "01" },
  { "DimmerStop",     0x0008, 0x07,   "" },
  { "HueMove",        0x0300, 0x01,   "xx19" },
  { "HueStep",        0x0300, 0x02,   "xx190A00" },
  { "SatMove",        0x0300, 0x04,   "xx19" },
  { "SatStep",        0x0300, 0x05,   "xx190A" },
  { "ColorMove",      0x0300, 0x08,   "xxxxyyyy" },
  { "ColorStep",      0x0300, 0x09,   "xxxxyyyy0A00" },
  // Tradfri
  { "ArrowClick",    0x0005, 0x07,   "xx" },         // xx == 0x01 = left, 0x00 = right
  { "ArrowHold",     0x0005, 0x08,   "xx" },         // xx == 0x01 = left, 0x00 = right
  { "ArrowRelease",  0x0005, 0x09,   "" },
};


#define ZLE(x) ((x) & 0xFF), ((x) >> 8)     // Little Endian

// Below are the attributes we wand to read from each cluster
const uint8_t CLUSTER_0006[] = { ZLE(0x0000) };    // Power
const uint8_t CLUSTER_0008[] = { ZLE(0x0000) };    // CurrentLevel
const uint8_t CLUSTER_0009[] = { ZLE(0x0000) };    // AlarmCount
const uint8_t CLUSTER_0300[] = { ZLE(0x0000), ZLE(0x0001), ZLE(0x0003), ZLE(0x0004), ZLE(0x0007) };    // Hue, Sat, X, Y, CT

// This callback is registered after a cluster specific command and sends a read command for the same cluster
int32_t Z_ReadAttrCallback(uint16_t shortaddr, uint16_t cluster, uint16_t endpoint, uint32_t value) {
  size_t         attrs_len = 0;
  const uint8_t* attrs = nullptr;

  switch (cluster) {
    case 0x0006:                              // for On/Off
      attrs = CLUSTER_0006;
      attrs_len = sizeof(CLUSTER_0006);
      break;
    case 0x0008:                              // for Dimmer
      attrs = CLUSTER_0008;
      attrs_len = sizeof(CLUSTER_0008);
      break;
    case 0x0009:                              // for Alarms
      attrs = CLUSTER_0009;
      attrs_len = sizeof(CLUSTER_0009);
      break;
    case 0x0300:                              // for Lights
      attrs = CLUSTER_0300;
      attrs_len = sizeof(CLUSTER_0300);
      break;
  }
  if (attrs) {
    ZigbeeZCLSend(shortaddr, cluster, endpoint, ZCL_READ_ATTRIBUTES, false, attrs, attrs_len, false /* we do want a response */);
  }
}

// set a timer to read back the value in the future
void zigbeeSetCommandTimer(uint16_t shortaddr, uint16_t cluster, uint16_t endpoint) {
  uint32_t wait_ms = 0;

  switch (cluster) {
    case 0x0006:        // for On/Off
    case 0x0009:        // for Alamrs
      wait_ms = 200;    // wait 0.2 s
      break;
    case 0x0008:        // for Dimmer
    case 0x0300:        // for Color
      wait_ms = 1050;   // wait 1.0 s
      break;
    case 0x0102:        // for Shutters
      wait_ms = 10000;  // wait 10.0 s
      break;
  }
  if (wait_ms) {
    zigbee_devices.setTimer(shortaddr, wait_ms, cluster, endpoint, 0 /* value */, &Z_ReadAttrCallback);
  }
}

inline bool isXYZ(char c) {
  return (c >= 'x') && (c <= 'z');
}

inline int8_t hexValue(char c) {
  if ((c >= '0') && (c <= '9')) {
    return c - '0';
  }
  if ((c >= 'A') && (c <= 'F')) {
    return 10 + c - 'A';
  }
  if ((c >= 'a') && (c <= 'f')) {
    return 10 + c - 'a';
  }
  return -1;
}

// works on big endiand hex only
uint32_t parseHex_P(const char **data, size_t max_len = 8) {
  uint32_t ret = 0;
  for (uint32_t i = 0; i < max_len; i++) {
    int8_t v = hexValue(pgm_read_byte(*data));
    if (v < 0) { break; }     // non hex digit, we stop parsing
    ret = (ret << 4) | v;
    *data += 1;
  }
  return ret;
}

void convertClusterSpecific(JsonObject& json, uint16_t cluster, uint8_t cmd, const SBuffer &payload) {
  char hex_char[payload.len()*2+2];
  ToHex_P((unsigned char*)payload.getBuffer(), payload.len(), hex_char, sizeof(hex_char));

  const __FlashStringHelper* command_name = nullptr;

//AddLog_P2(LOG_LEVEL_INFO, PSTR(">>> len = %d - %02X%02X%02X"), payload.len(), payload.get8(0), payload.get8(1), payload.get8(2));
  for (uint32_t i = 0; i < sizeof(Z_Commands) / sizeof(Z_Commands[0]); i++) {
    const Z_CommandConverter *conv = &Z_Commands[i];
    if (conv->cluster == cluster) {
      // cluster match
      if ((0xFFFF == conv->cmd) || (cmd == conv->cmd)) {
        // cmd match
        // check if we have a match for params too
        // Match if:
        //  - payload exactly matches conv->param (conv->param may be longer)
        //  - payload matches conv->param until 'x', 'y' or 'z'
        const char * p = conv->param;
//AddLog_P2(LOG_LEVEL_INFO, PSTR(">>>++1 param = %s"), p);
        bool match = true;
        for (uint8_t i = 0; i < payload.len(); i++) {
          const char c1 = pgm_read_byte(p);
          const char c2 = pgm_read_byte(p+1);
//AddLog_P2(LOG_LEVEL_INFO, PSTR(">>>++2 c1 = %c, c2 = %c"), c1, c2);
          if ((0x00 == c1) || isXYZ(c1)) {
            break;
          }
          const char * p2 = p;
          uint32_t nextbyte = parseHex_P(&p2, 2);
//AddLog_P2(LOG_LEVEL_INFO, PSTR(">>>++3 parseHex_P = %02X"), nextbyte);
          if (nextbyte != payload.get8(i)) {
            match = false;
            break;
          }
          p += 2;
        }
        if (match) {
          // parse xyz
          command_name = (const __FlashStringHelper*) conv->tasmota_cmd;
          break;
        }
      }
    }


  }

  if (command_name) {
    json[command_name] = true;
  } else {
    char attrid_str[12];
    snprintf_P(attrid_str, sizeof(attrid_str), PSTR("%04X!%02X"), cluster, cmd);

    json[attrid_str] = hex_char;
  }
}

// Find the command details by command name
// Returns if found:
//  - cluster number
//  - command number or 0xFFFF if command is part of the variable part
//  - the payload in the form of a HEX string with x/y/z variables
// If not found:
//  - returns nullptr
const __FlashStringHelper* zigbeeFindCommand(const char *command, uint16_t *cluster, uint16_t *cmd) {
  for (uint32_t i = 0; i < sizeof(Z_Commands) / sizeof(Z_Commands[0]); i++) {
    const Z_CommandConverter *conv = &Z_Commands[i];
    if (0 == strcasecmp_P(command, conv->tasmota_cmd)) {
      *cluster = conv->cluster;
      *cmd = conv->cmd;
      return (const __FlashStringHelper*) conv->param;
    }
  }

  return nullptr;
}

// take the lower 4 bits and turn it to an hex char
inline char hexDigit(uint32_t h) {
  uint32_t nybble = h & 0x0F;
  return (nybble > 9) ? 'A' - 10 + nybble : '0' + nybble;
}

// replace all xx/yy/zz substrings with unsigned ints, and the corresponding len (8, 16 or 32 bits)
String zigbeeCmdAddParams(const char *zcl_cmd_P, uint32_t x, uint32_t y, uint32_t z) {
  size_t len = strlen_P(zcl_cmd_P);
  char zcl_cmd[len+1];
  strcpy_P(zcl_cmd, zcl_cmd_P);     // copy into RAM

  char *p = zcl_cmd;
  while (*p) {
    if (isXYZ(*p) && (*p == *(p+1))) {    // if char is [x-z] and followed by same char
      uint8_t val;
      switch (*p) {
        case 'x':
          val = x & 0xFF;
          x = x >> 8;
          break;
        case 'y':
          val = y & 0xFF;
          y = y >> 8;
          break;
        case 'z':
          val = z & 0xFF;
          z = z >> 8;
          break;
      }
      *p = hexDigit(val >> 4);
      *(p+1) = hexDigit(val);
      p++;
    }
    p++;
  }
  AddLog_P2(LOG_LEVEL_DEBUG, PSTR("SendZCLCommand_P: zcl_cmd = %s"), zcl_cmd);

  return String(zcl_cmd);
}

const char kZ_Alias[] PROGMEM = "OFF|" D_OFF "|" D_FALSE "|" D_STOP  "|" "OPEN" "|"           // 0
                                "ON|"  D_ON  "|" D_TRUE  "|" D_START "|" "CLOSE" "|"          // 1
                                "TOGGLE|" D_TOGGLE "|"                                        // 2
                                "ALL" ;                                                       // 255

const uint8_t kZ_Numbers[] PROGMEM = { 0,0,0,0,0,
                                       1,1,1,1,1,
                                       2,2,
                                       255 };

// Convert an alias like "On" to the corresponding number
uint32_t ZigbeeAliasOrNumber(const char *state_text) {
  char command[16];
  int state_number = GetCommandCode(command, sizeof(command), state_text, kZ_Alias);
  if (state_number >= 0) {
    // found an alias, get its value
    return pgm_read_byte(kZ_Numbers + state_number);
  } else {
    // no alias found, convert it as number
    return strtoul(state_text, nullptr, 0);
  }
}

#endif // USE_ZIGBEE
