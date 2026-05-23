#include "shell.hpp"

#include <string.h>

namespace shell {

bool handleLowLevelCommand(char *argv[], size_t argc) {
#if FEATURE_LOWLEVEL
  if (argc > 0 && strcmp(argv[0], "ddr") == 0) {
    if (argc != 2 && argc != 3) {
      Serial.println(F("Usage: ddr <port> [value]"));
      Serial.println(F("Ports: b|c|d"));
      return true;
    }
    PortId portId = PortId::B;
    if (!parsePortId(argv[1], portId)) {
      Serial.println(F("Invalid port. Use b|c|d."));
      return true;
    }
    volatile uint8_t &reg = ddrForPort(portId);
    if (argc == 3) {
      uint8_t value = 0;
      if (!parseByteValue(argv[2], value)) {
        Serial.println(F("Invalid value. Use 0..255 (decimal or 0x..)."));
        return true;
      }
      reg = value;
    }

    Serial.print(F("DDR"));
    Serial.print(portLetter(portId));
    Serial.print(F(" = 0x"));
    printHexByte(reg);
    Serial.print(F(" ("));
    Serial.print(reg);
    Serial.println(F(")"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "port") == 0) {
    if (argc != 2 && argc != 3) {
      Serial.println(F("Usage: port <port> [value]"));
      Serial.println(F("Ports: b|c|d"));
      return true;
    }
    PortId portId = PortId::B;
    if (!parsePortId(argv[1], portId)) {
      Serial.println(F("Invalid port. Use b|c|d."));
      return true;
    }
    volatile uint8_t &reg = portForPort(portId);
    if (argc == 3) {
      uint8_t value = 0;
      if (!parseByteValue(argv[2], value)) {
        Serial.println(F("Invalid value. Use 0..255 (decimal or 0x..)."));
        return true;
      }
      reg = value;
    }

    Serial.print(F("PORT"));
    Serial.print(portLetter(portId));
    Serial.print(F(" = 0x"));
    printHexByte(reg);
    Serial.print(F(" ("));
    Serial.print(reg);
    Serial.println(F(")"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "pin") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: pin <port>"));
      Serial.println(F("Ports: b|c|d"));
      return true;
    }
    PortId portId = PortId::B;
    if (!parsePortId(argv[1], portId)) {
      Serial.println(F("Invalid port. Use b|c|d."));
      return true;
    }
    volatile uint8_t &reg = pinForPort(portId);
    Serial.print(F("PIN"));
    Serial.print(portLetter(portId));
    Serial.print(F(" = 0x"));
    printHexByte(reg);
    Serial.print(F(" ("));
    Serial.print(reg);
    Serial.println(F(")"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "peek") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: peek <addr>"));
      Serial.println(F("Address: 0..65535 or 0x0000..0xFFFF"));
      return true;
    }

    uint16_t addr = 0;
    if (!parseAddressValue(argv[1], addr)) {
      Serial.println(F("Invalid address. Use 0..65535 or 0x...."));
      return true;
    }

    volatile uint8_t *const ptr = reinterpret_cast<volatile uint8_t *>(addr);
    const uint8_t value = *ptr;

    Serial.print(F("[0x"));
    printHexWord(addr);
    Serial.print(F("] = 0x"));
    printHexByte(value);
    Serial.print(F(" ("));
    Serial.print(value);
    Serial.println(F(")"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "poke") == 0) {
    if (argc != 3) {
      Serial.println(F("Usage: poke <addr> <val>"));
      Serial.println(F("Addr: 0..65535 or 0x0000..0xFFFF"));
      Serial.println(F("Val: 0..255 or 0x00..0xFF"));
      return true;
    }

    uint16_t addr = 0;
    if (!parseAddressValue(argv[1], addr)) {
      Serial.println(F("Invalid address. Use 0..65535 or 0x...."));
      return true;
    }

    uint8_t value = 0;
    if (!parseByteValue(argv[2], value)) {
      Serial.println(F("Invalid value. Use 0..255 or 0x.."));
      return true;
    }

    volatile uint8_t *const ptr = reinterpret_cast<volatile uint8_t *>(addr);
    *ptr = value;
    const uint8_t readBack = *ptr;

    Serial.print(F("[0x"));
    printHexWord(addr);
    Serial.print(F("] <= 0x"));
    printHexByte(value);
    Serial.print(F(" (readback 0x"));
    printHexByte(readBack);
    Serial.println(F(")"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "reg") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: reg"));
      return true;
    }

    Serial.println(F("\n=== AVR Registers ==="));
    Serial.print(F("SP   : 0x"));
    printHexWord(SP);
    Serial.println();

    Serial.print(F("SPL  : 0x"));
    printHexByte(SPL);
    Serial.print(F("  SPH: 0x"));
    printHexByte(SPH);
    Serial.println();

    Serial.print(F("SREG : 0x"));
    printHexByte(SREG);
    Serial.print(F("  MCUSR(now): 0x"));
    printHexByte(MCUSR);
    Serial.print(F("  MCUSR(boot): 0x"));
    printHexByte(gResetFlags);
    Serial.println();

    Serial.print(F("DDRB : 0x"));
    printHexByte(DDRB);
    Serial.print(F("  PORTB: 0x"));
    printHexByte(PORTB);
    Serial.print(F("  PINB: 0x"));
    printHexByte(PINB);
    Serial.println();

    Serial.print(F("DDRC : 0x"));
    printHexByte(DDRC);
    Serial.print(F("  PORTC: 0x"));
    printHexByte(PORTC);
    Serial.print(F("  PINC: 0x"));
    printHexByte(PINC);
    Serial.println();

    Serial.print(F("DDRD : 0x"));
    printHexByte(DDRD);
    Serial.print(F("  PORTD: 0x"));
    printHexByte(PORTD);
    Serial.print(F("  PIND: 0x"));
    printHexByte(PIND);
    Serial.println();

    Serial.println(F("=====================\n"));
    return true;
  }
#else
  (void)argv;
  (void)argc;
#endif

  return false;
}

} // namespace shell
