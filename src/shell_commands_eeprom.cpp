#include "shell.hpp"

#include <EEPROM.h>
#include <string.h>

namespace shell {

bool handleEepromCommand(char *argv[], size_t argc) {
#if FEATURE_EEPROM
  if (argc > 0 && strcmp(argv[0], "eepread") == 0) {
    if (argc != 2 && argc != 3) {
      Serial.println(F("Usage: eepread <addr> [len]"));
      return true;
    }

    const size_t size = eepromSize();
    uint16_t address = 0;
    if (!parseEepromAddress(argv[1], address)) {
      Serial.print(F("Invalid EEPROM address. Use 0.."));
      Serial.println(size - 1);
      return true;
    }

    size_t length = 1;
    if (argc == 3 && !parseEepromLen(argv[2], length)) {
      Serial.println(F("Invalid length. Use >= 1."));
      return true;
    }

    const size_t start = static_cast<size_t>(address);
    if (length > (size - start)) {
      Serial.println(F("Read range exceeds EEPROM."));
      return true;
    }

    Serial.print(F("EEPROM read "));
    Serial.print(length);
    Serial.print(F(" byte(s) @ 0x"));
    printHexWord(address);
    Serial.println();

    for (size_t i = 0; i < length; ++i) {
      const size_t index = start + i;
      if ((i % 16) == 0) {
        Serial.print(F("0x"));
        printHexWord(static_cast<uint16_t>(index));
        Serial.print(F(":"));
      }

      Serial.write(' ');
      printHexByte(EEPROM.read(static_cast<int>(index)));

      if ((i % 16) == 15 || (i + 1) == length) {
        Serial.println();
      }
    }
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "eepwrite") == 0) {
    if (argc < 3) {
      Serial.println(F("Usage: eepwrite <addr> <bytes...>"));
      return true;
    }

    const size_t size = eepromSize();
    uint16_t address = 0;
    if (!parseEepromAddress(argv[1], address)) {
      Serial.print(F("Invalid EEPROM address. Use 0.."));
      Serial.println(size - 1);
      return true;
    }

    const size_t start = static_cast<size_t>(address);
    const size_t dataLen = argc - 2;
    if (dataLen > (size - start)) {
      Serial.println(F("Write range exceeds EEPROM."));
      return true;
    }

    uint8_t data[kMaxArgs] = {};
    for (size_t i = 0; i < dataLen; ++i) {
      if (!parseByteValue(argv[2 + i], data[i])) {
        Serial.print(F("Invalid byte: "));
        Serial.println(argv[2 + i]);
        return true;
      }
    }

    for (size_t i = 0; i < dataLen; ++i) {
      EEPROM.update(static_cast<int>(start + i), data[i]);
    }

    Serial.print(F("EEPROM wrote "));
    Serial.print(dataLen);
    Serial.print(F(" byte(s) @ 0x"));
    printHexWord(address);
    Serial.println();
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "eeperase") == 0) {
    if (argc != 2 || strcmp(argv[1], kEepromEraseToken) != 0) {
      Serial.print(F("Usage: eeperase "));
      Serial.println(kEepromEraseToken);
      return true;
    }

    const size_t size = eepromSize();
    for (size_t i = 0; i < size; ++i) {
      EEPROM.update(static_cast<int>(i), kEepromEraseValue);
    }

    Serial.print(F("EEPROM cleared to 0x"));
    printHexByte(kEepromEraseValue);
    Serial.print(F(" ("));
    Serial.print(size);
    Serial.println(F(" bytes)."));
    return true;
  }
#else
  (void)argv;
  (void)argc;
#endif

  return false;
}

} // namespace shell
