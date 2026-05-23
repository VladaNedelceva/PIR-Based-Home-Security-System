#include "shell.hpp"

#include <EEPROM.h>
#include <avr/boot.h>
#include <avr/wdt.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

extern "C" char __heap_start;
extern "C" void *__brkval;

namespace shell {

uint8_t gResetFlags = 0;
#if FEATURE_I2C
uint32_t gI2cClockHz = kI2cSpeed100kHz;
#endif

const char kEepromEraseToken[] = "confirm";

char gCmdBuffer[kCmdBufferSize];
size_t gCmdLen = 0;
char gHistory[kHistorySize][kCmdBufferSize];
size_t gHistoryCount = 0;
size_t gHistoryHead = 0;
int gHistoryCursor = -1;
char gEditBackup[kCmdBufferSize];
size_t gEditBackupLen = 0;
EscState gEscState = EscState::None;

void printPrompt() { Serial.print(F("arduino$ ")); }

void print2Digits(uint32_t value) {
  if (value < 10) {
    Serial.write('0');
  }
  Serial.print(value);
}

void print3Digits(uint32_t value) {
  if (value < 100) {
    Serial.write('0');
  }
  if (value < 10) {
    Serial.write('0');
  }
  Serial.print(value);
}

void printHexByte(uint8_t value) {
  const char hex[] = "0123456789ABCDEF";
  Serial.write(hex[(value >> 4) & 0x0F]);
  Serial.write(hex[value & 0x0F]);
}

void printHexWord(uint16_t value) {
  printHexByte(static_cast<uint8_t>(value >> 8));
  printHexByte(static_cast<uint8_t>(value & 0xFF));
}

void printUptimeFormatted(uint32_t ms) {
  const uint32_t totalSeconds = ms / 1000UL;
  const uint32_t days = totalSeconds / 86400UL;
  const uint32_t hours = (totalSeconds / 3600UL) % 24UL;
  const uint32_t minutes = (totalSeconds / 60UL) % 60UL;
  const uint32_t seconds = totalSeconds % 60UL;
  const uint32_t millisPart = ms % 1000UL;

  if (days > 0) {
    Serial.print(days);
    Serial.print(F("d "));
  }
  print2Digits(hours);
  Serial.write(':');
  print2Digits(minutes);
  Serial.write(':');
  print2Digits(seconds);
  Serial.write('.');
  print3Digits(millisPart);
}

void readDeviceSignature(uint8_t outSig[3]) {
  const uint8_t stepped[3] = {
    boot_signature_byte_get(0x00),
    boot_signature_byte_get(0x02),
    boot_signature_byte_get(0x04)};
  outSig[0] = stepped[0];
  outSig[1] = stepped[1];
  outSig[2] = stepped[2];
}

void printResetCause() {
  bool first = true;
  if (gResetFlags & _BV(PORF)) {
    Serial.print(F("POR"));
    first = false;
  }
  if (gResetFlags & _BV(EXTRF)) {
    if (!first) {
      Serial.print(F(", "));
    }
    Serial.print(F("EXTR"));
    first = false;
  }
  if (gResetFlags & _BV(BORF)) {
    if (!first) {
      Serial.print(F(", "));
    }
    Serial.print(F("BOR"));
    first = false;
  }
  if (gResetFlags & _BV(WDRF)) {
    if (!first) {
      Serial.print(F(", "));
    }
    Serial.print(F("WDR"));
    first = false;
  }
  if (first) {
    Serial.print(F("unknown"));
  }
}

void captureResetFlags() {
  gResetFlags = MCUSR;
  MCUSR = 0;
  wdt_disable();
}

int freeRamEstimate() {
  int v;
  return (int)&v - (int)(__brkval == nullptr ? &__heap_start : __brkval);
}

bool startsWithIgnoreCase(const char *text, const char *prefix) {
  while (*prefix != '\0') {
    if (*text == '\0') {
      return false;
    }
    const char tc = static_cast<char>(tolower(static_cast<unsigned char>(*text)));
    const char pc = static_cast<char>(tolower(static_cast<unsigned char>(*prefix)));
    if (tc != pc) {
      return false;
    }
    ++text;
    ++prefix;
  }
  return true;
}

bool equalsIgnoreCase(const char *a, const char *b) {
  if (a == nullptr || b == nullptr) {
    return false;
  }
  while (*a != '\0' && *b != '\0') {
    const char ac = static_cast<char>(tolower(static_cast<unsigned char>(*a)));
    const char bc = static_cast<char>(tolower(static_cast<unsigned char>(*b)));
    if (ac != bc) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

size_t splitArgs(char *text, char *argv[], size_t maxArgs) {
  size_t argc = 0;
  char *p = text;
  while (*p != '\0' && argc < maxArgs) {
    while (*p == ' ') {
      ++p;
    }
    if (*p == '\0') {
      break;
    }
    argv[argc++] = p;
    while (*p != '\0' && *p != ' ') {
      ++p;
    }
    if (*p == ' ') {
      *p = '\0';
      ++p;
    }
  }
  return argc;
}

bool parseUnsigned(const char *token, unsigned long &value) {
  if (token == nullptr || *token == '\0') {
    return false;
  }
  char *end = nullptr;
  value = strtoul(token, &end, 10);
  return *end == '\0';
}

bool parseUnsignedAuto(const char *token, unsigned long &value) {
  if (token == nullptr || *token == '\0' || token[0] == '-') {
    return false;
  }
  char *end = nullptr;
  value = strtoul(token, &end, 0);
  return *end == '\0';
}

bool parseByteValue(const char *token, uint8_t &value) {
  unsigned long raw = 0;
  if (!parseUnsignedAuto(token, raw) || raw > 0xFFUL) {
    return false;
  }
  value = static_cast<uint8_t>(raw);
  return true;
}

#if FEATURE_LOWLEVEL
bool parseAddressValue(const char *token, uint16_t &value) {
  unsigned long raw = 0;
  if (!parseUnsignedAuto(token, raw) || raw > 0xFFFFUL) {
    return false;
  }
  value = static_cast<uint16_t>(raw);
  return true;
}
#endif

#if FEATURE_I2C
bool parseI2cAddress(const char *token, uint8_t &address) {
  uint8_t raw = 0;
  if (!parseByteValue(token, raw) || raw > 0x7FU) {
    return false;
  }
  address = raw;
  return true;
}

bool parseI2cSpeedToken(const char *token, uint32_t &hz) {
  if (token == nullptr || *token == '\0') {
    return false;
  }

  if (strcmp(token, "100k") == 0 || strcmp(token, "100") == 0 ||
      strcmp(token, "100000") == 0) {
    hz = kI2cSpeed100kHz;
    return true;
  }
  if (strcmp(token, "400k") == 0 || strcmp(token, "400") == 0 ||
      strcmp(token, "400000") == 0) {
    hz = kI2cSpeed400kHz;
    return true;
  }
  return false;
}

bool parseI2cLen(const char *token, uint8_t &length) {
  unsigned long raw = 0;
  if (!parseUnsignedAuto(token, raw) || raw == 0 || raw > kI2cMaxTransferLen) {
    return false;
  }
  length = static_cast<uint8_t>(raw);
  return true;
}
#endif

size_t eepromSize() { return static_cast<size_t>(EEPROM.length()); }

#if FEATURE_EEPROM
bool parseEepromAddress(const char *token, uint16_t &address) {
  unsigned long raw = 0;
  if (!parseUnsignedAuto(token, raw)) {
    return false;
  }

  const size_t size = eepromSize();
  if (raw >= size) {
    return false;
  }

  address = static_cast<uint16_t>(raw);
  return true;
}

bool parseEepromLen(const char *token, size_t &length) {
  unsigned long raw = 0;
  if (!parseUnsignedAuto(token, raw) || raw == 0) {
    return false;
  }
  length = static_cast<size_t>(raw);
  return true;
}
#endif

uint16_t eepromReadU16(size_t addr) {
  const uint16_t lo = EEPROM.read(static_cast<int>(addr));
  const uint16_t hi = EEPROM.read(static_cast<int>(addr + 1U));
  return static_cast<uint16_t>(lo | (hi << 8));
}

void eepromWriteU16(size_t addr, uint16_t value) {
  EEPROM.update(static_cast<int>(addr), static_cast<uint8_t>(value & 0xFFU));
  EEPROM.update(static_cast<int>(addr + 1U), static_cast<uint8_t>((value >> 8) & 0xFFU));
}

size_t fsEntryAddress(uint8_t index) {
  return static_cast<size_t>(kFsEntryTableOffset) +
         (static_cast<size_t>(index) * static_cast<size_t>(kFsEntrySize));
}

bool fsIsValidNameToken(const char *name) {
  if (name == nullptr || *name == '\0') {
    return false;
  }
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
    return false;
  }

  const size_t len = strlen(name);
  if (len == 0 || len >= kFsNameBytes) {
    return false;
  }

  for (size_t i = 0; i < len; ++i) {
    const unsigned char c = static_cast<unsigned char>(name[i]);
    if (c < 32 || c == '/') {
      return false;
    }
  }
  return true;
}

void fsSetRootEntry(FsEntry &entry) {
  entry.used = true;
  entry.isDir = true;
  entry.parent = kFsRootParent;
  strncpy(entry.name, "/", kFsNameBytes - 1);
  entry.name[kFsNameBytes - 1] = '\0';
  entry.dataStart = 0;
  entry.dataLen = 0;
}

void fsLoadEntry(uint8_t index, FsEntry &entry) {
  const size_t base = fsEntryAddress(index);
  const uint8_t flags = EEPROM.read(static_cast<int>(base));
  entry.used = (flags & 0x01U) != 0U;
  entry.isDir = (flags & 0x02U) != 0U;
  entry.parent = EEPROM.read(static_cast<int>(base + 1U));

  for (size_t i = 0; i < kFsNameBytes; ++i) {
    entry.name[i] = static_cast<char>(EEPROM.read(static_cast<int>(base + 2U + i)));
  }
  entry.name[kFsNameBytes - 1] = '\0';

  entry.dataStart = eepromReadU16(base + 14U);
  entry.dataLen = eepromReadU16(base + 16U);
}

void fsStoreEntry(uint8_t index, const FsEntry &entry) {
  const size_t base = fsEntryAddress(index);
  uint8_t flags = 0;
  if (entry.used) {
    flags |= 0x01U;
  }
  if (entry.isDir) {
    flags |= 0x02U;
  }

  EEPROM.update(static_cast<int>(base), flags);
  EEPROM.update(static_cast<int>(base + 1U), entry.parent);

  size_t nameLen = strlen(entry.name);
  if (nameLen > (kFsNameBytes - 1U)) {
    nameLen = kFsNameBytes - 1U;
  }
  for (size_t i = 0; i < kFsNameBytes; ++i) {
    const char c = (i < nameLen) ? entry.name[i] : '\0';
    EEPROM.update(static_cast<int>(base + 2U + i), static_cast<uint8_t>(c));
  }

  eepromWriteU16(base + 14U, entry.dataStart);
  eepromWriteU16(base + 16U, entry.dataLen);
  EEPROM.update(static_cast<int>(base + 18U), 0);
  EEPROM.update(static_cast<int>(base + 19U), 0);
}

void fsClearEntry(uint8_t index) {
  const size_t base = fsEntryAddress(index);
  for (size_t i = 0; i < kFsEntrySize; ++i) {
    EEPROM.update(static_cast<int>(base + i), 0);
  }
}

uint16_t fsNextFree() { return eepromReadU16(8U); }

void fsSetNextFree(uint16_t nextFree) { eepromWriteU16(8U, nextFree); }

bool fsIsFormatted() {
  const size_t size = eepromSize();
  if (size <= kFsDataStart) {
    return false;
  }

  if (EEPROM.read(0) != kFsMagic0 || EEPROM.read(1) != kFsMagic1 ||
      EEPROM.read(2) != kFsMagic2 || EEPROM.read(3) != kFsMagic3) {
    return false;
  }
  if (EEPROM.read(4) != kFsVersion) {
    return false;
  }
  if (EEPROM.read(5) != kFsMaxEntries) {
    return false;
  }
  if (eepromReadU16(6U) != kFsDataStart) {
    return false;
  }

  const uint16_t nextFree = fsNextFree();
  return nextFree >= kFsDataStart && nextFree <= size;
}

void fsFormat() {
  EEPROM.update(0, kFsMagic0);
  EEPROM.update(1, kFsMagic1);
  EEPROM.update(2, kFsMagic2);
  EEPROM.update(3, kFsMagic3);
  EEPROM.update(4, kFsVersion);
  EEPROM.update(5, kFsMaxEntries);
  eepromWriteU16(6U, kFsDataStart);
  fsSetNextFree(kFsDataStart);
  for (size_t i = 10; i < kFsHeaderSize; ++i) {
    EEPROM.update(static_cast<int>(i), 0);
  }

  for (uint8_t i = 0; i < kFsMaxEntries; ++i) {
    fsClearEntry(i);
  }
}

#if FEATURE_FS
void fsEnsureInitialized() {
  if (!fsIsFormatted()) {
    fsFormat();
  }
}
#endif

bool fsFindChild(uint8_t parent, const char *name, uint8_t &indexOut, FsEntry &entryOut) {
  for (uint8_t i = 0; i < kFsMaxEntries; ++i) {
    FsEntry entry;
    fsLoadEntry(i, entry);
    if (entry.used && entry.parent == parent && strcmp(entry.name, name) == 0) {
      indexOut = i;
      entryOut = entry;
      return true;
    }
  }
  return false;
}

bool fsFindFreeEntry(uint8_t &indexOut) {
  for (uint8_t i = 0; i < kFsMaxEntries; ++i) {
    FsEntry entry;
    fsLoadEntry(i, entry);
    if (!entry.used) {
      indexOut = i;
      return true;
    }
  }
  return false;
}

bool fsHasChildren(uint8_t parentIndex) {
  for (uint8_t i = 0; i < kFsMaxEntries; ++i) {
    FsEntry entry;
    fsLoadEntry(i, entry);
    if (entry.used && entry.parent == parentIndex) {
      return true;
    }
  }
  return false;
}

bool fsResolvePath(const char *path, uint8_t &indexOut, FsEntry &entryOut) {
  if (path == nullptr) {
    return false;
  }

  char work[kCmdBufferSize];
  strncpy(work, path, kCmdBufferSize - 1);
  work[kCmdBufferSize - 1] = '\0';

  char *start = work;
  while (*start != '\0' && isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }

  size_t len = strlen(start);
  while (len > 0 && isspace(static_cast<unsigned char>(start[len - 1]))) {
    start[len - 1] = '\0';
    --len;
  }

  while (len > 1 && start[len - 1] == '/') {
    start[len - 1] = '\0';
    --len;
  }

  if (len == 0 || (len == 1 && start[0] == '/')) {
    indexOut = kFsRootParent;
    fsSetRootEntry(entryOut);
    return true;
  }

  while (*start == '/') {
    ++start;
  }
  if (*start == '\0') {
    indexOut = kFsRootParent;
    fsSetRootEntry(entryOut);
    return true;
  }

  uint8_t currentParent = kFsRootParent;
  uint8_t currentIndex = kFsRootParent;
  FsEntry currentEntry;

  char *saveptr = nullptr;
  char *token = strtok_r(start, "/", &saveptr);
  while (token != nullptr) {
    if (!fsIsValidNameToken(token)) {
      return false;
    }
    if (!fsFindChild(currentParent, token, currentIndex, currentEntry)) {
      return false;
    }

    token = strtok_r(nullptr, "/", &saveptr);
    if (token != nullptr && !currentEntry.isDir) {
      return false;
    }
    currentParent = currentIndex;
  }

  indexOut = currentIndex;
  entryOut = currentEntry;
  return true;
}

bool fsResolveDirectory(const char *path, uint8_t &indexOut, FsEntry &entryOut) {
  if (!fsResolvePath(path, indexOut, entryOut)) {
    return false;
  }
  return entryOut.isDir;
}

bool fsSplitParentLeaf(const char *path, char *parentOut, size_t parentOutSize, char *leafOut,
                       size_t leafOutSize) {
  if (path == nullptr || parentOut == nullptr || leafOut == nullptr || parentOutSize == 0 ||
      leafOutSize == 0) {
    return false;
  }

  char work[kCmdBufferSize];
  strncpy(work, path, kCmdBufferSize - 1);
  work[kCmdBufferSize - 1] = '\0';

  char *start = work;
  while (*start != '\0' && isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }

  size_t len = strlen(start);
  while (len > 0 && isspace(static_cast<unsigned char>(start[len - 1]))) {
    start[len - 1] = '\0';
    --len;
  }

  while (len > 1 && start[len - 1] == '/') {
    start[len - 1] = '\0';
    --len;
  }

  if (len == 0 || (len == 1 && start[0] == '/')) {
    return false;
  }

  char *lastSlash = strrchr(start, '/');
  const char *leaf = nullptr;
  const char *parent = nullptr;

  if (lastSlash == nullptr) {
    parent = "/";
    leaf = start;
  } else {
    *lastSlash = '\0';
    leaf = lastSlash + 1;
    parent = (start[0] == '\0') ? "/" : start;
  }

  if (!fsIsValidNameToken(leaf)) {
    return false;
  }

  const size_t leafLen = strlen(leaf);
  const size_t parentLen = strlen(parent);
  if (leafLen >= leafOutSize || parentLen >= parentOutSize) {
    return false;
  }

  strncpy(leafOut, leaf, leafOutSize - 1);
  leafOut[leafOutSize - 1] = '\0';
  strncpy(parentOut, parent, parentOutSize - 1);
  parentOut[parentOutSize - 1] = '\0';
  return true;
}

#if FEATURE_I2C
void setI2cClock(uint32_t hz) {
#if defined(TWBR) && defined(TWPS0) && defined(TWPS1) && defined(F_CPU)
  // Force prescaler=1 and compute TWBR from AVR datasheet formula.
  TWSR = static_cast<uint8_t>(TWSR & ~(_BV(TWPS0) | _BV(TWPS1)));
  const uint32_t twbrValue = ((F_CPU / hz) - 16UL) / 2UL;
  TWBR = static_cast<uint8_t>(twbrValue);
#else
  Wire.setClock(hz);
#endif
  gI2cClockHz = hz;
}
#endif

#if FEATURE_I2C
void printI2cAddress(uint8_t address) {
  Serial.print(F("0x"));
  printHexByte(address);
}

void printI2cTxStatus(uint8_t status) {
  Serial.print(F("I2C error "));
  Serial.print(status);
  Serial.print(F(" ("));
  switch (status) {
    case 1:
      Serial.print(F("data too long"));
      break;
    case 2:
      Serial.print(F("NACK on address"));
      break;
    case 3:
      Serial.print(F("NACK on data"));
      break;
    case 4:
      Serial.print(F("other bus error"));
      break;
    case 5:
      Serial.print(F("timeout"));
      break;
    default:
      Serial.print(F("unknown"));
      break;
  }
  Serial.println(F(")"));
}
#endif

#if FEATURE_LOWLEVEL
bool parsePortId(const char *token, PortId &port) {
  if (token == nullptr || *token == '\0') {
    return false;
  }

  if (strcmp(token, "b") == 0 || strcmp(token, "portb") == 0 ||
      strcmp(token, "ddrb") == 0 || strcmp(token, "pinb") == 0) {
    port = PortId::B;
    return true;
  }
  if (strcmp(token, "c") == 0 || strcmp(token, "portc") == 0 ||
      strcmp(token, "ddrc") == 0 || strcmp(token, "pinc") == 0) {
    port = PortId::C;
    return true;
  }
  if (strcmp(token, "d") == 0 || strcmp(token, "portd") == 0 ||
      strcmp(token, "ddrd") == 0 || strcmp(token, "pind") == 0) {
    port = PortId::D;
    return true;
  }
  return false;
}

char portLetter(PortId port) {
  switch (port) {
    case PortId::B:
      return 'B';
    case PortId::C:
      return 'C';
    case PortId::D:
      return 'D';
  }
  return '?';
}

volatile uint8_t &ddrForPort(PortId port) {
  switch (port) {
    case PortId::B:
      return DDRB;
    case PortId::C:
      return DDRC;
    case PortId::D:
      return DDRD;
  }
  return DDRB;
}

volatile uint8_t &portForPort(PortId port) {
  switch (port) {
    case PortId::B:
      return PORTB;
    case PortId::C:
      return PORTC;
    case PortId::D:
      return PORTD;
  }
  return PORTB;
}

volatile uint8_t &pinForPort(PortId port) {
  switch (port) {
    case PortId::B:
      return PINB;
    case PortId::C:
      return PINC;
    case PortId::D:
      return PIND;
  }
  return PINB;
}
#endif

bool parsePinToken(const char *token, int &pin) {
  if (token == nullptr || *token == '\0') {
    return false;
  }
  if (token[0] == 'a' || token[0] == 'A') {
    unsigned long idx = 0;
    if (!parseUnsigned(token + 1, idx) || idx >= kUserAnalogCount) {
      return false;
    }
    pin = A0 + static_cast<int>(idx);
    return true;
  }

  unsigned long rawPin = 0;
  if (!parseUnsigned(token, rawPin) || rawPin >= NUM_DIGITAL_PINS) {
    return false;
  }
  pin = static_cast<int>(rawPin);
  return true;
}

bool parseAnalogPinToken(const char *token, uint8_t &analogIndex, int &pin) {
  if (token == nullptr || *token == '\0') {
    return false;
  }

  if (token[0] == 'a' || token[0] == 'A') {
    unsigned long idx = 0;
    if (!parseUnsigned(token + 1, idx) || idx >= kUserAnalogCount) {
      return false;
    }
    analogIndex = static_cast<uint8_t>(idx);
    pin = A0 + static_cast<int>(idx);
    return true;
  }

  unsigned long raw = 0;
  if (!parseUnsigned(token, raw)) {
    return false;
  }
  if (raw < kUserAnalogCount) {
    analogIndex = static_cast<uint8_t>(raw);
    pin = A0 + static_cast<int>(raw);
    return true;
  }
  if (raw >= A0 && raw < (A0 + kUserAnalogCount)) {
    analogIndex = static_cast<uint8_t>(raw - A0);
    pin = static_cast<int>(raw);
    return true;
  }
  return false;
}

void printPinLabel(int pin) {
  if (pin >= A0 && pin < (A0 + kUserAnalogCount)) {
    Serial.print('A');
    Serial.print(pin - A0);
    Serial.print(F("/"));
  }
  Serial.print('D');
  Serial.print(pin);
}

bool isPwmCapablePin(int pin) {
#if defined(digitalPinHasPWM)
  return digitalPinHasPWM(pin);
#else
  return pin == 3 || pin == 5 || pin == 6 || pin == 9 || pin == 10 || pin == 11;
#endif
}

void setCmdBuffer(const char *text) {
  strncpy(gCmdBuffer, text, kCmdBufferSize - 1);
  gCmdBuffer[kCmdBufferSize - 1] = '\0';
  gCmdLen = strlen(gCmdBuffer);
}

void redrawInputLine(size_t previousLen) {
  Serial.write('\r');
  printPrompt();
  for (size_t i = 0; i < gCmdLen; ++i) {
    Serial.write(gCmdBuffer[i]);
  }

  if (previousLen > gCmdLen) {
    const size_t extra = previousLen - gCmdLen;
    for (size_t i = 0; i < extra; ++i) {
      Serial.write(' ');
    }
    for (size_t i = 0; i < extra; ++i) {
      Serial.write('\b');
    }
  }
}

const char *historyEntryFromNewest(size_t newestOffset) {
  const size_t idx = (gHistoryHead + kHistorySize - 1 - newestOffset) % kHistorySize;
  return gHistory[idx];
}

void pushHistory(const char *line) {
  if (line[0] == '\0') {
    return;
  }

  if (gHistoryCount > 0 && strcmp(historyEntryFromNewest(0), line) == 0) {
    return;
  }

  strncpy(gHistory[gHistoryHead], line, kCmdBufferSize - 1);
  gHistory[gHistoryHead][kCmdBufferSize - 1] = '\0';
  gHistoryHead = (gHistoryHead + 1) % kHistorySize;
  if (gHistoryCount < kHistorySize) {
    ++gHistoryCount;
  }
}

void resetHistoryBrowse() {
  gHistoryCursor = -1;
  gEditBackupLen = 0;
  gEditBackup[0] = '\0';
}

void historyUp() {
  if (gHistoryCount == 0) {
    return;
  }

  const size_t previousLen = gCmdLen;
  if (gHistoryCursor < 0) {
    strncpy(gEditBackup, gCmdBuffer, kCmdBufferSize - 1);
    gEditBackup[kCmdBufferSize - 1] = '\0';
    gEditBackupLen = gCmdLen;
    gHistoryCursor = 0;
  } else if ((size_t)(gHistoryCursor + 1) < gHistoryCount) {
    ++gHistoryCursor;
  }

  setCmdBuffer(historyEntryFromNewest((size_t)gHistoryCursor));
  redrawInputLine(previousLen);
}

void historyDown() {
  if (gHistoryCursor < 0) {
    return;
  }

  const size_t previousLen = gCmdLen;
  if (gHistoryCursor > 0) {
    --gHistoryCursor;
    setCmdBuffer(historyEntryFromNewest((size_t)gHistoryCursor));
  } else {
    gHistoryCursor = -1;
    setCmdBuffer(gEditBackup);
    gCmdLen = gEditBackupLen;
    gCmdBuffer[gCmdLen] = '\0';
  }

  redrawInputLine(previousLen);
}


} // namespace shell
