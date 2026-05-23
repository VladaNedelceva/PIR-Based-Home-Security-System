#pragma once

#include <Arduino.h>
#include <Wire.h>

namespace shell {

#ifndef DEMO_BAUD
#define DEMO_BAUD 57600UL
#endif
constexpr uint32_t kBaudRate = DEMO_BAUD;
constexpr size_t kCmdBufferSize = 64;
constexpr size_t kMaxArgs = 32;
constexpr size_t kHistorySize = 8;
constexpr uint16_t kWatchPeriodMs = 200;
constexpr uint16_t kDefaultFreqWindowMs = 250;
constexpr uint16_t kMinFreqWindowMs = 10;
constexpr uint16_t kMaxFreqWindowMs = 10000;
constexpr uint8_t kI2cMaxTransferLen = 32;
constexpr uint32_t kI2cSpeed100kHz = 100000UL;
constexpr uint32_t kI2cSpeed400kHz = 400000UL;
constexpr uint8_t kEepromEraseValue = 0xFF;
extern const char kEepromEraseToken[];
constexpr uint8_t kFsMagic0 = 'E';
constexpr uint8_t kFsMagic1 = 'F';
constexpr uint8_t kFsMagic2 = 'S';
constexpr uint8_t kFsMagic3 = '1';
constexpr uint8_t kFsVersion = 1;
constexpr uint8_t kFsRootParent = 0xFF;
constexpr uint8_t kFsMaxEntries = 16;
constexpr uint8_t kFsNameBytes = 12;
constexpr uint8_t kFsEntrySize = 20;
constexpr uint16_t kFsHeaderSize = 16;
constexpr uint16_t kFsEntryTableOffset = kFsHeaderSize;
constexpr uint16_t kFsDataStart =
    kFsEntryTableOffset + (static_cast<uint16_t>(kFsMaxEntries) * kFsEntrySize);
constexpr uint8_t kUserAnalogCount = 6;

#ifndef FW_VERSION
#define FW_VERSION "1.1.0"
#endif

#ifndef FEATURE_I2C
#define FEATURE_I2C 1
#endif

#ifndef FEATURE_EEPROM
#define FEATURE_EEPROM 1
#endif

#ifndef FEATURE_FS
#define FEATURE_FS 1
#endif

#ifndef FEATURE_TONE
#define FEATURE_TONE 1
#endif

#ifndef FEATURE_LOWLEVEL
#define FEATURE_LOWLEVEL 1
#endif

#if FEATURE_FS && !FEATURE_EEPROM
#error "FEATURE_FS requires FEATURE_EEPROM=1"
#endif

enum class EscState : uint8_t { None, SeenEsc, SeenEscBracket };

struct FsEntry {
  bool used = false;
  bool isDir = false;
  uint8_t parent = kFsRootParent;
  char name[kFsNameBytes] = {0};
  uint16_t dataStart = 0;
  uint16_t dataLen = 0;
};

#if FEATURE_LOWLEVEL
enum class PortId : uint8_t { B, C, D };
#endif

extern uint8_t gResetFlags;
#if FEATURE_I2C
extern uint32_t gI2cClockHz;
#endif

extern char gCmdBuffer[kCmdBufferSize];
extern size_t gCmdLen;
extern char gHistory[kHistorySize][kCmdBufferSize];
extern size_t gHistoryCount;
extern size_t gHistoryHead;
extern int gHistoryCursor;
extern char gEditBackup[kCmdBufferSize];
extern size_t gEditBackupLen;
extern EscState gEscState;

void printPrompt();
void print2Digits(uint32_t value);
void print3Digits(uint32_t value);
void printHexByte(uint8_t value);
void printHexWord(uint16_t value);
void printUptimeFormatted(uint32_t ms);
void readDeviceSignature(uint8_t outSig[3]);
void printResetCause();
void captureResetFlags();
int freeRamEstimate();

bool startsWithIgnoreCase(const char *text, const char *prefix);
bool equalsIgnoreCase(const char *a, const char *b);
size_t splitArgs(char *text, char *argv[], size_t maxArgs);
bool parseUnsigned(const char *token, unsigned long &value);
bool parseUnsignedAuto(const char *token, unsigned long &value);
bool parseByteValue(const char *token, uint8_t &value);

#if FEATURE_LOWLEVEL
bool parseAddressValue(const char *token, uint16_t &value);
#endif

#if FEATURE_I2C
bool parseI2cAddress(const char *token, uint8_t &address);
bool parseI2cSpeedToken(const char *token, uint32_t &hz);
bool parseI2cLen(const char *token, uint8_t &length);
#endif

size_t eepromSize();

#if FEATURE_EEPROM
bool parseEepromAddress(const char *token, uint16_t &address);
bool parseEepromLen(const char *token, size_t &length);
#endif

uint16_t eepromReadU16(size_t addr);
void eepromWriteU16(size_t addr, uint16_t value);
size_t fsEntryAddress(uint8_t index);
bool fsIsValidNameToken(const char *name);
void fsSetRootEntry(FsEntry &entry);
void fsLoadEntry(uint8_t index, FsEntry &entry);
void fsStoreEntry(uint8_t index, const FsEntry &entry);
void fsClearEntry(uint8_t index);
uint16_t fsNextFree();
void fsSetNextFree(uint16_t nextFree);
bool fsIsFormatted();
void fsFormat();

#if FEATURE_FS
void fsEnsureInitialized();
#endif

bool fsFindChild(uint8_t parent, const char *name, uint8_t &indexOut, FsEntry &entryOut);
bool fsFindFreeEntry(uint8_t &indexOut);
bool fsHasChildren(uint8_t parentIndex);
bool fsResolvePath(const char *path, uint8_t &indexOut, FsEntry &entryOut);
bool fsResolveDirectory(const char *path, uint8_t &indexOut, FsEntry &entryOut);
bool fsSplitParentLeaf(const char *path, char *parentOut, size_t parentOutSize, char *leafOut,
                       size_t leafOutSize);

#if FEATURE_I2C
void setI2cClock(uint32_t hz);
void printI2cAddress(uint8_t address);
void printI2cTxStatus(uint8_t status);
#endif

#if FEATURE_LOWLEVEL
bool parsePortId(const char *token, PortId &port);
char portLetter(PortId port);
volatile uint8_t &ddrForPort(PortId port);
volatile uint8_t &portForPort(PortId port);
volatile uint8_t &pinForPort(PortId port);
#endif

bool parsePinToken(const char *token, int &pin);
bool parseAnalogPinToken(const char *token, uint8_t &analogIndex, int &pin);
void printPinLabel(int pin);
bool isPwmCapablePin(int pin);

void setCmdBuffer(const char *text);
void redrawInputLine(size_t previousLen);
const char *historyEntryFromNewest(size_t newestOffset);
void pushHistory(const char *line);
void resetHistoryBrowse();
void historyUp();
void historyDown();

void printHelp();
void printStatus();
#if FEATURE_FS
void handleFsCommand(const char *rawLine);
#endif
bool handleI2cCommand(char *argv[], size_t argc);
bool handleEepromCommand(char *argv[], size_t argc);
bool handleGpioCommand(char *argv[], size_t argc);
bool handleLowLevelCommand(char *argv[], size_t argc);
void handleCommand(char *line);
void updateSerial();
void startupScriptInit();
void updateBackgroundTasks();

} // namespace shell
