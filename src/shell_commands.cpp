#include "shell.hpp"

#include <avr/wdt.h>
#include <ctype.h>
#include <string.h>

namespace shell {

namespace {

const __FlashStringHelper *selectedBoardName() {
#if defined(ARDUINO_AVR_UNO)
  return F("Arduino Uno");
#elif defined(ARDUINO_AVR_ATmega328P)
  return F("ATmega328P Xplained Mini");
#else
  return F("ATmega328P-compatible board");
#endif
}

void normalize(char *s) {
  size_t w = 0;
  bool prevSpace = true;

  for (size_t r = 0; s[r] != '\0'; ++r) {
    char c = static_cast<char>(tolower(static_cast<unsigned char>(s[r])));
    if (c == '\t') {
      c = ' ';
    }
    if (c == ' ') {
      if (prevSpace) {
        continue;
      }
      prevSpace = true;
      s[w++] = c;
    } else {
      prevSpace = false;
      s[w++] = c;
    }
  }

  if (w > 0 && s[w - 1] == ' ') {
    --w;
  }
  s[w] = '\0';
}

} // namespace

void handleCommand(char *line) {
  char raw[kCmdBufferSize];
  strncpy(raw, line, kCmdBufferSize - 1);
  raw[kCmdBufferSize - 1] = '\0';

  char *trimmed = raw;
  while (*trimmed != '\0' && isspace(static_cast<unsigned char>(*trimmed))) {
    ++trimmed;
  }

  size_t end = strlen(trimmed);
  while (end > 0 && isspace(static_cast<unsigned char>(trimmed[end - 1]))) {
    trimmed[end - 1] = '\0';
    --end;
  }

  if (trimmed[0] == '\0') {
    return;
  }

#if FEATURE_FS
  if (startsWithIgnoreCase(trimmed, "fs")) {
    const char next = trimmed[2];
    if (next == '\0' || isspace(static_cast<unsigned char>(next))) {
      handleFsCommand(trimmed);
      return;
    }
  }
#endif

  if (startsWithIgnoreCase(trimmed, "echo")) {
    const char *text = trimmed + 4;
    if (*text != '\0' && !isspace(static_cast<unsigned char>(*text))) {
      Serial.print(F("Unknown command: "));
      Serial.println(trimmed);
      Serial.println(F("Type 'help'"));
      return;
    }
    while (*text != '\0' && isspace(static_cast<unsigned char>(*text))) {
      ++text;
    }
    Serial.println(text);
    return;
  }

  normalize(trimmed);

  char *argv[kMaxArgs] = {};
  const size_t argc = splitArgs(trimmed, argv, kMaxArgs);
  if (argc == 0) {
    return;
  }

  if (strcmp(argv[0], "help") == 0 && argc == 1) {
    printHelp();
    return;
  }
  if (strcmp(argv[0], "status") == 0 && argc == 1) {
    printStatus();
    return;
  }
  if (strcmp(argv[0], "ver") == 0 && argc == 1) {
    Serial.println(F("\n=== Firmware Info ==="));
    Serial.print(F("Version: "));
    Serial.println(FW_VERSION);
    Serial.print(F("Build: "));
    Serial.print(F(__DATE__));
    Serial.write(' ');
    Serial.println(F(__TIME__));
    Serial.print(F("Board: "));
    Serial.println(selectedBoardName());
    Serial.print(F("MCU: "));
    Serial.println(F("ATmega328P"));
    Serial.print(F("F_CPU: "));
    Serial.print(F_CPU);
    Serial.println(F(" Hz"));
    Serial.print(F("UART baud: "));
    Serial.println(kBaudRate);
    Serial.print(F("Compiler: "));
    Serial.println(__VERSION__);
    Serial.print(F("Reset cause: "));
    printResetCause();
    Serial.println();
    Serial.println(F("=====================\n"));
    return;
  }
  if (strcmp(argv[0], "id") == 0 && argc == 1) {
    uint8_t sig[3] = {0, 0, 0};
    readDeviceSignature(sig);
    Serial.print(F("Board: "));
    Serial.println(selectedBoardName());
    Serial.print(F("Device ID: 0x"));
    printHexByte(sig[0]);
    printHexByte(sig[1]);
    printHexByte(sig[2]);
    Serial.println();
    return;
  }
  if (strcmp(argv[0], "uptime") == 0 && argc == 1) {
    const uint32_t upMs = millis();
    Serial.print(F("Uptime: "));
    printUptimeFormatted(upMs);
    Serial.print(F(" ("));
    Serial.print(upMs);
    Serial.println(F(" ms)"));
    return;
  }
  if (strcmp(argv[0], "free") == 0 && argc == 1) {
    Serial.print(F("Free RAM (estimate): "));
    Serial.print(freeRamEstimate());
    Serial.println(F(" bytes"));
    return;
  }
  if (strcmp(argv[0], "micros") == 0 && argc == 1) {
    Serial.print(F("micros(): "));
    Serial.println(micros());
    return;
  }
  if (strcmp(argv[0], "reset") == 0 && argc == 1) {
    Serial.println(F("Resetting via watchdog..."));
    Serial.flush();
    delay(20);
    wdt_enable(WDTO_15MS);
    while (true) {
    }
  }

  if (handleI2cCommand(argv, argc)) {
    return;
  }
  if (handleEepromCommand(argv, argc)) {
    return;
  }
  if (handleGpioCommand(argv, argc)) {
    return;
  }
  if (handleLowLevelCommand(argv, argc)) {
    return;
  }

  Serial.print(F("Unknown command: "));
  Serial.println(trimmed);
  Serial.println(F("Type 'help'"));
}

} // namespace shell
