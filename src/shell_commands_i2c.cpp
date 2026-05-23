#include "shell.hpp"

#include <string.h>

namespace shell {

bool handleI2cCommand(char *argv[], size_t argc) {
#if FEATURE_I2C
  if (argc > 0 && strcmp(argv[0], "i2cspeed") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: i2cspeed <100k|400k>"));
      return true;
    }

    uint32_t hz = 0;
    if (!parseI2cSpeedToken(argv[1], hz)) {
      Serial.println(F("Invalid speed. Use 100k or 400k."));
      return true;
    }

    setI2cClock(hz);
    Serial.print(F("I2C speed set to "));
    Serial.print(gI2cClockHz / 1000UL);
    Serial.println(F(" kHz"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "i2cscan") == 0) {
    if (argc != 1) {
      Serial.println(F("Usage: i2cscan"));
      return true;
    }

    uint8_t found = 0;
    Serial.println(F("Scanning I2C addresses 0x01..0x7F..."));
    for (uint8_t address = 1; address <= 0x7F; ++address) {
      Wire.beginTransmission(address);
      const uint8_t status = Wire.endTransmission();
      if (status == 0) {
        Serial.print(F("  found @ "));
        printI2cAddress(address);
        Serial.println();
        ++found;
      } else if (status == 4) {
        Serial.print(F("  bus error @ "));
        printI2cAddress(address);
        Serial.println();
      }
    }

    if (found == 0) {
      Serial.println(F("No I2C devices found."));
    } else {
      Serial.print(F("I2C devices found: "));
      Serial.println(found);
    }
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "i2cread") == 0) {
    if (argc != 3) {
      Serial.println(F("Usage: i2cread <addr> <n>"));
      Serial.print(F("n range: 1.."));
      Serial.println(kI2cMaxTransferLen);
      return true;
    }

    uint8_t address = 0;
    uint8_t length = 0;
    if (!parseI2cAddress(argv[1], address)) {
      Serial.println(F("Invalid address. Use 0x00..0x7F."));
      return true;
    }
    if (!parseI2cLen(argv[2], length)) {
      Serial.print(F("Invalid length. Use 1.."));
      Serial.println(kI2cMaxTransferLen);
      return true;
    }

    const uint8_t received =
        Wire.requestFrom(static_cast<int>(address), static_cast<int>(length));
    Serial.print(F("i2cread "));
    printI2cAddress(address);
    Serial.print(F(" -> "));
    Serial.print(received);
    Serial.print(F(" byte(s):"));

    for (uint8_t i = 0; i < received && Wire.available() > 0; ++i) {
      const uint8_t value = static_cast<uint8_t>(Wire.read());
      Serial.write(' ');
      Serial.print(F("0x"));
      printHexByte(value);
    }
    Serial.println();

    if (received != length) {
      Serial.print(F("Short read (requested "));
      Serial.print(length);
      Serial.println(F(")."));
    }
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "i2cwrite") == 0) {
    if (argc < 3) {
      Serial.println(F("Usage: i2cwrite <addr> <bytes...>"));
      return true;
    }

    uint8_t address = 0;
    if (!parseI2cAddress(argv[1], address)) {
      Serial.println(F("Invalid address. Use 0x00..0x7F."));
      return true;
    }

    const size_t dataLen = argc - 2;
    if (dataLen == 0 || dataLen > kI2cMaxTransferLen) {
      Serial.print(F("Data length must be 1.."));
      Serial.println(kI2cMaxTransferLen);
      return true;
    }

    uint8_t data[kI2cMaxTransferLen] = {};
    for (size_t i = 0; i < dataLen; ++i) {
      if (!parseByteValue(argv[2 + i], data[i])) {
        Serial.print(F("Invalid data byte: "));
        Serial.println(argv[2 + i]);
        return true;
      }
    }

    Wire.beginTransmission(address);
    for (size_t i = 0; i < dataLen; ++i) {
      Wire.write(data[i]);
    }
    const uint8_t status = Wire.endTransmission();
    if (status != 0) {
      printI2cTxStatus(status);
      return true;
    }

    Serial.print(F("Wrote "));
    Serial.print(dataLen);
    Serial.print(F(" byte(s) to "));
    printI2cAddress(address);
    Serial.println();
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "i2cwr") == 0) {
    if (argc < 4) {
      Serial.println(F("Usage: i2cwr <addr> <reg> <bytes...>"));
      return true;
    }

    uint8_t address = 0;
    uint8_t reg = 0;
    if (!parseI2cAddress(argv[1], address)) {
      Serial.println(F("Invalid address. Use 0x00..0x7F."));
      return true;
    }
    if (!parseByteValue(argv[2], reg)) {
      Serial.println(F("Invalid register. Use 0..255 or 0x00..0xFF."));
      return true;
    }

    const size_t dataLen = argc - 3;
    if (dataLen == 0 || (1 + dataLen) > kI2cMaxTransferLen) {
      Serial.print(F("Payload too long. reg + data must be <= "));
      Serial.print(kI2cMaxTransferLen);
      Serial.println(F(" bytes."));
      return true;
    }

    uint8_t data[kI2cMaxTransferLen - 1] = {};
    for (size_t i = 0; i < dataLen; ++i) {
      if (!parseByteValue(argv[3 + i], data[i])) {
        Serial.print(F("Invalid data byte: "));
        Serial.println(argv[3 + i]);
        return true;
      }
    }

    Wire.beginTransmission(address);
    Wire.write(reg);
    for (size_t i = 0; i < dataLen; ++i) {
      Wire.write(data[i]);
    }
    const uint8_t status = Wire.endTransmission();
    if (status != 0) {
      printI2cTxStatus(status);
      return true;
    }

    Serial.print(F("Wrote reg 0x"));
    printHexByte(reg);
    Serial.print(F(" + "));
    Serial.print(dataLen);
    Serial.print(F(" byte(s) to "));
    printI2cAddress(address);
    Serial.println();
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "i2crr") == 0) {
    if (argc != 4) {
      Serial.println(F("Usage: i2crr <addr> <reg> <n>"));
      Serial.print(F("n range: 1.."));
      Serial.println(kI2cMaxTransferLen);
      return true;
    }

    uint8_t address = 0;
    uint8_t reg = 0;
    uint8_t length = 0;
    if (!parseI2cAddress(argv[1], address)) {
      Serial.println(F("Invalid address. Use 0x00..0x7F."));
      return true;
    }
    if (!parseByteValue(argv[2], reg)) {
      Serial.println(F("Invalid register. Use 0..255 or 0x00..0xFF."));
      return true;
    }
    if (!parseI2cLen(argv[3], length)) {
      Serial.print(F("Invalid length. Use 1.."));
      Serial.println(kI2cMaxTransferLen);
      return true;
    }

    Wire.beginTransmission(address);
    Wire.write(reg);
    const uint8_t txStatus = Wire.endTransmission(false);
    if (txStatus != 0) {
      printI2cTxStatus(txStatus);
      return true;
    }

    const uint8_t received =
        Wire.requestFrom(static_cast<int>(address), static_cast<int>(length));
    Serial.print(F("i2crr "));
    printI2cAddress(address);
    Serial.print(F(" reg 0x"));
    printHexByte(reg);
    Serial.print(F(" -> "));
    Serial.print(received);
    Serial.print(F(" byte(s):"));

    for (uint8_t i = 0; i < received && Wire.available() > 0; ++i) {
      const uint8_t value = static_cast<uint8_t>(Wire.read());
      Serial.write(' ');
      Serial.print(F("0x"));
      printHexByte(value);
    }
    Serial.println();

    if (received != length) {
      Serial.print(F("Short read (requested "));
      Serial.print(length);
      Serial.println(F(")."));
    }
    return true;
  }
#else
  (void)argv;
  (void)argc;
#endif

  return false;
}

} // namespace shell
