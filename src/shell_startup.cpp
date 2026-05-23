#include "shell.hpp"

#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <ctype.h>
#include <string.h>

namespace shell {

namespace {

const char kDefaultBootScriptPgm[] PROGMEM =
    "# Startup script\n"
    "# blink <pin> <period_ms>\n"
    "blink 13 1000\n";

bool gBlinkEnabled = false;
uint8_t gBlinkPin = 13;
bool gBlinkLevelHigh = false;
uint16_t gBlinkHighMs = 500;
uint16_t gBlinkLowMs = 500;
uint32_t gBlinkNextToggleMs = 0;

void setBlinkTask(int pin, uint16_t periodMs) {
  if (periodMs < 2) {
    periodMs = 2;
  }

  uint16_t highMs = static_cast<uint16_t>(periodMs / 2U);
  uint16_t lowMs = static_cast<uint16_t>(periodMs - highMs);
  if (highMs == 0) {
    highMs = 1;
  }
  if (lowMs == 0) {
    lowMs = 1;
  }

  gBlinkPin = pin;
  gBlinkHighMs = highMs;
  gBlinkLowMs = lowMs;
  gBlinkLevelHigh = false;
  gBlinkEnabled = true;

  pinMode(gBlinkPin, OUTPUT);
  digitalWrite(gBlinkPin, LOW);
  gBlinkNextToggleMs = millis() + gBlinkLowMs;
}

bool ensureScriptsDirectory() {
  char scriptsDirName[] = "scripts";
  uint8_t existingIndex = 0;
  FsEntry existingEntry;
  if (fsFindChild(kFsRootParent, scriptsDirName, existingIndex, existingEntry)) {
    return existingEntry.isDir;
  }

  uint8_t newIndex = 0;
  if (!fsFindFreeEntry(newIndex)) {
    return false;
  }

  FsEntry dirEntry;
  dirEntry.used = true;
  dirEntry.isDir = true;
  dirEntry.parent = kFsRootParent;
  strncpy(dirEntry.name, scriptsDirName, kFsNameBytes - 1);
  dirEntry.name[kFsNameBytes - 1] = '\0';
  dirEntry.dataStart = 0;
  dirEntry.dataLen = 0;
  fsStoreEntry(newIndex, dirEntry);
  return true;
}

bool ensureDefaultBootScript() {
  char bootScriptPath[] = "/scripts/boot.sh";
  uint8_t nodeIndex = kFsRootParent;
  FsEntry nodeEntry;
  if (fsResolvePath(bootScriptPath, nodeIndex, nodeEntry)) {
    return !nodeEntry.isDir;
  }

  char parentPath[kCmdBufferSize];
  char leaf[kFsNameBytes];
  if (!fsSplitParentLeaf(bootScriptPath, parentPath, sizeof(parentPath), leaf, sizeof(leaf))) {
    return false;
  }

  uint8_t parentIndex = kFsRootParent;
  FsEntry parentEntry;
  if (!fsResolveDirectory(parentPath, parentIndex, parentEntry)) {
    return false;
  }

  uint8_t freeIndex = 0;
  if (!fsFindFreeEntry(freeIndex)) {
    return false;
  }

  const size_t textLen = strlen_P(kDefaultBootScriptPgm);
  const size_t size = eepromSize();
  const uint16_t nextFree = fsNextFree();
  if (nextFree > size || textLen > (size - nextFree)) {
    return false;
  }

  for (size_t i = 0; i < textLen; ++i) {
    const uint8_t c = static_cast<uint8_t>(pgm_read_byte(kDefaultBootScriptPgm + i));
    EEPROM.update(static_cast<int>(nextFree + i), c);
  }

  FsEntry fileEntry;
  fileEntry.used = true;
  fileEntry.isDir = false;
  fileEntry.parent = parentIndex;
  strncpy(fileEntry.name, leaf, kFsNameBytes - 1);
  fileEntry.name[kFsNameBytes - 1] = '\0';
  fileEntry.dataStart = nextFree;
  fileEntry.dataLen = static_cast<uint16_t>(textLen);
  fsStoreEntry(freeIndex, fileEntry);
  fsSetNextFree(static_cast<uint16_t>(nextFree + textLen));
  return true;
}

void trimInPlace(char *line) {
  if (line == nullptr) {
    return;
  }

  char *start = line;
  while (*start != '\0' && isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }

  if (start != line) {
    memmove(line, start, strlen(start) + 1U);
  }

  size_t len = strlen(line);
  while (len > 0 && isspace(static_cast<unsigned char>(line[len - 1]))) {
    line[len - 1] = '\0';
    --len;
  }
}

void executeBootScriptLine(char *line) {
  trimInPlace(line);
  if (line[0] == '\0' || line[0] == '#') {
    return;
  }

  char *argv[4] = {};
  const size_t argc = splitArgs(line, argv, 4);
  if (argc != 3 || !equalsIgnoreCase(argv[0], "blink")) {
    return;
  }

  int pin = -1;
  unsigned long periodMs = 0;
  if (!parsePinToken(argv[1], pin)) {
    return;
  }
  if (!parseUnsignedAuto(argv[2], periodMs) || periodMs > 60000UL) {
    return;
  }

  setBlinkTask(pin, static_cast<uint16_t>(periodMs));
}

void runBootScript() {
  char bootScriptPath[] = "/scripts/boot.sh";
  uint8_t nodeIndex = kFsRootParent;
  FsEntry entry;
  if (!fsResolvePath(bootScriptPath, nodeIndex, entry) || entry.isDir || entry.dataLen == 0) {
    return;
  }

  char line[kCmdBufferSize];
  size_t lineLen = 0;

  for (uint16_t i = 0; i < entry.dataLen; ++i) {
    const char c = static_cast<char>(EEPROM.read(static_cast<int>(entry.dataStart + i)));
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      line[lineLen] = '\0';
      executeBootScriptLine(line);
      lineLen = 0;
      continue;
    }
    if (lineLen < (kCmdBufferSize - 1U)) {
      line[lineLen++] = c;
    }
  }

  if (lineLen > 0) {
    line[lineLen] = '\0';
    executeBootScriptLine(line);
  }
}

} // namespace

void startupScriptInit() {
#if FEATURE_FS
  if (!fsIsFormatted()) {
    return;
  }
  if (!ensureScriptsDirectory()) {
    return;
  }
  if (!ensureDefaultBootScript()) {
    return;
  }
  runBootScript();
#endif
}

void updateBackgroundTasks() {
  if (!gBlinkEnabled) {
    return;
  }

  const uint32_t now = millis();
  if (static_cast<int32_t>(now - gBlinkNextToggleMs) < 0) {
    return;
  }

  gBlinkLevelHigh = !gBlinkLevelHigh;
  digitalWrite(gBlinkPin, gBlinkLevelHigh ? HIGH : LOW);
  gBlinkNextToggleMs = now + (gBlinkLevelHigh ? gBlinkHighMs : gBlinkLowMs);
}

} // namespace shell
