#include "shell.hpp"

#include <EEPROM.h>
#include <ctype.h>
#include <string.h>

namespace shell {

#if FEATURE_FS
void printFsHelp() {
  Serial.println(F("\nFS commands:"));
  Serial.println(F("  fs help"));
  Serial.println(F("  fs format confirm"));
  Serial.println(F("  fs ls [path]"));
  Serial.println(F("  fs cat <path>"));
  Serial.println(F("  fs mkdir <path>"));
  Serial.println(F("  fs touch <path>"));
  Serial.println(F("  fs write <path> <text>"));
  Serial.println(F("  fs rm <path>"));
  Serial.println(F("  fs stat"));
  Serial.println();
}

void handleFsCommand(const char *rawLine) {
  if (rawLine == nullptr) {
    return;
  }

  char argsLine[kCmdBufferSize];
  strncpy(argsLine, rawLine, kCmdBufferSize - 1);
  argsLine[kCmdBufferSize - 1] = '\0';

  // FS subcommands are short; keep this argv small to reduce stack pressure.
  char *argv[8] = {};
  const size_t argc = splitArgs(argsLine, argv, 8);
  if (argc == 0 || !equalsIgnoreCase(argv[0], "fs")) {
    return;
  }

  if (argc == 1 || equalsIgnoreCase(argv[1], "help")) {
    printFsHelp();
    return;
  }

  if (equalsIgnoreCase(argv[1], "format")) {
    if (argc != 3 || !equalsIgnoreCase(argv[2], kEepromEraseToken)) {
      Serial.print(F("Usage: fs format "));
      Serial.println(kEepromEraseToken);
      return;
    }
    fsFormat();
    Serial.print(F("FS formatted. Capacity: "));
    Serial.print(eepromSize() - kFsDataStart);
    Serial.println(F(" bytes data."));
    return;
  }

  if (!fsIsFormatted()) {
    Serial.print(F("FS not initialized. Run: fs format "));
    Serial.println(kEepromEraseToken);
    return;
  }

  if (equalsIgnoreCase(argv[1], "ls")) {
    if (argc != 2 && argc != 3) {
      Serial.println(F("Usage: fs ls [path]"));
      return;
    }
    const char *path = (argc == 3) ? argv[2] : "/";
    uint8_t dirIndex = kFsRootParent;
    FsEntry dirEntry;
    if (!fsResolveDirectory(path, dirIndex, dirEntry)) {
      Serial.println(F("Path is not a directory or does not exist."));
      return;
    }

    Serial.print(F("Listing "));
    Serial.println(path);

    uint8_t shown = 0;
    for (uint8_t i = 0; i < kFsMaxEntries; ++i) {
      FsEntry entry;
      fsLoadEntry(i, entry);
      if (!entry.used || entry.parent != dirIndex) {
        continue;
      }
      ++shown;
      Serial.print(entry.isDir ? F("d ") : F("f "));
      Serial.print(entry.name);
      if (!entry.isDir) {
        Serial.print(F(" ("));
        Serial.print(entry.dataLen);
        Serial.print(F("B)"));
      }
      Serial.println();
    }
    if (shown == 0) {
      Serial.println(F("(empty)"));
    }
    return;
  }

  if (equalsIgnoreCase(argv[1], "cat")) {
    if (argc != 3) {
      Serial.println(F("Usage: fs cat <path>"));
      return;
    }
    uint8_t nodeIndex = kFsRootParent;
    FsEntry entry;
    if (!fsResolvePath(argv[2], nodeIndex, entry) || entry.isDir) {
      Serial.println(F("File not found."));
      return;
    }

    if (entry.dataLen == 0) {
      Serial.println(F("(empty file)"));
      return;
    }

    for (uint16_t i = 0; i < entry.dataLen; ++i) {
      const uint8_t value = EEPROM.read(static_cast<int>(entry.dataStart + i));
      if (value == '\n' || value == '\r' || value == '\t' || isprint(value)) {
        Serial.write(value);
      } else {
        Serial.print(F("\\x"));
        printHexByte(value);
      }
    }
    Serial.println();
    return;
  }

  if (equalsIgnoreCase(argv[1], "mkdir")) {
    if (argc != 3) {
      Serial.println(F("Usage: fs mkdir <path>"));
      return;
    }

    char parentPath[kCmdBufferSize];
    char leaf[kFsNameBytes];
    if (!fsSplitParentLeaf(argv[2], parentPath, sizeof(parentPath), leaf, sizeof(leaf))) {
      Serial.println(F("Invalid path."));
      return;
    }

    uint8_t parentIndex = kFsRootParent;
    FsEntry parentEntry;
    if (!fsResolveDirectory(parentPath, parentIndex, parentEntry)) {
      Serial.println(F("Parent directory does not exist."));
      return;
    }

    uint8_t existingIndex = 0;
    FsEntry existingEntry;
    if (fsFindChild(parentIndex, leaf, existingIndex, existingEntry)) {
      Serial.println(F("Path already exists."));
      return;
    }

    uint8_t newIndex = 0;
    if (!fsFindFreeEntry(newIndex)) {
      Serial.println(F("FS entry table full."));
      return;
    }

    FsEntry newEntry;
    newEntry.used = true;
    newEntry.isDir = true;
    newEntry.parent = parentIndex;
    strncpy(newEntry.name, leaf, kFsNameBytes - 1);
    newEntry.name[kFsNameBytes - 1] = '\0';
    fsStoreEntry(newIndex, newEntry);

    Serial.print(F("Directory created: "));
    Serial.println(argv[2]);
    return;
  }

  if (equalsIgnoreCase(argv[1], "touch")) {
    if (argc != 3) {
      Serial.println(F("Usage: fs touch <path>"));
      return;
    }

    char parentPath[kCmdBufferSize];
    char leaf[kFsNameBytes];
    if (!fsSplitParentLeaf(argv[2], parentPath, sizeof(parentPath), leaf, sizeof(leaf))) {
      Serial.println(F("Invalid path."));
      return;
    }

    uint8_t parentIndex = kFsRootParent;
    FsEntry parentEntry;
    if (!fsResolveDirectory(parentPath, parentIndex, parentEntry)) {
      Serial.println(F("Parent directory does not exist."));
      return;
    }

    uint8_t nodeIndex = 0;
    FsEntry nodeEntry;
    if (fsFindChild(parentIndex, leaf, nodeIndex, nodeEntry)) {
      if (nodeEntry.isDir) {
        Serial.println(F("Path exists as directory."));
        return;
      }
      Serial.println(F("File already exists."));
      return;
    }

    if (!fsFindFreeEntry(nodeIndex)) {
      Serial.println(F("FS entry table full."));
      return;
    }

    FsEntry newEntry;
    newEntry.used = true;
    newEntry.isDir = false;
    newEntry.parent = parentIndex;
    strncpy(newEntry.name, leaf, kFsNameBytes - 1);
    newEntry.name[kFsNameBytes - 1] = '\0';
    fsStoreEntry(nodeIndex, newEntry);
    Serial.print(F("File created: "));
    Serial.println(argv[2]);
    return;
  }

  if (equalsIgnoreCase(argv[1], "write")) {
    // Parse path + raw text from the original command to preserve text case and spacing.
    const char *p = rawLine;
    while (*p != '\0' && isspace(static_cast<unsigned char>(*p))) {
      ++p;
    }
    while (*p != '\0' && !isspace(static_cast<unsigned char>(*p))) {
      ++p; // fs
    }
    while (*p != '\0' && isspace(static_cast<unsigned char>(*p))) {
      ++p;
    }
    while (*p != '\0' && !isspace(static_cast<unsigned char>(*p))) {
      ++p; // write
    }
    while (*p != '\0' && isspace(static_cast<unsigned char>(*p))) {
      ++p;
    }
    if (*p == '\0') {
      Serial.println(F("Usage: fs write <path> <text>"));
      return;
    }

    const char *pathStart = p;
    while (*p != '\0' && !isspace(static_cast<unsigned char>(*p))) {
      ++p;
    }
    const size_t pathLen = static_cast<size_t>(p - pathStart);
    if (pathLen == 0 || pathLen >= kCmdBufferSize) {
      Serial.println(F("Invalid path."));
      return;
    }

    char path[kCmdBufferSize];
    memcpy(path, pathStart, pathLen);
    path[pathLen] = '\0';

    while (*p != '\0' && isspace(static_cast<unsigned char>(*p))) {
      ++p;
    }
    const char *text = p; // May be empty.
    const size_t textLen = strlen(text);

    char parentPath[kCmdBufferSize];
    char leaf[kFsNameBytes];
    if (!fsSplitParentLeaf(path, parentPath, sizeof(parentPath), leaf, sizeof(leaf))) {
      Serial.println(F("Invalid path."));
      return;
    }

    uint8_t parentIndex = kFsRootParent;
    FsEntry parentEntry;
    if (!fsResolveDirectory(parentPath, parentIndex, parentEntry)) {
      Serial.println(F("Parent directory does not exist."));
      return;
    }

    uint8_t nodeIndex = 0;
    FsEntry nodeEntry;
    bool exists = fsFindChild(parentIndex, leaf, nodeIndex, nodeEntry);
    if (exists && nodeEntry.isDir) {
      Serial.println(F("Path exists as directory."));
      return;
    }
    if (!exists) {
      if (!fsFindFreeEntry(nodeIndex)) {
        Serial.println(F("FS entry table full."));
        return;
      }
      nodeEntry.used = true;
      nodeEntry.isDir = false;
      nodeEntry.parent = parentIndex;
      strncpy(nodeEntry.name, leaf, kFsNameBytes - 1);
      nodeEntry.name[kFsNameBytes - 1] = '\0';
    }

    if (textLen == 0) {
      nodeEntry.dataLen = 0;
      nodeEntry.dataStart = 0;
      fsStoreEntry(nodeIndex, nodeEntry);
      Serial.print(F("Wrote 0 bytes to "));
      Serial.println(path);
      return;
    }

    const size_t size = eepromSize();
    const uint16_t nextFree = fsNextFree();
    if (nextFree > size || textLen > (size - nextFree)) {
      Serial.println(F("Not enough EEPROM data space. Run 'fs format confirm'."));
      return;
    }

    for (size_t i = 0; i < textLen; ++i) {
      EEPROM.update(static_cast<int>(nextFree + i), static_cast<uint8_t>(text[i]));
    }

    nodeEntry.dataStart = nextFree;
    nodeEntry.dataLen = static_cast<uint16_t>(textLen);
    fsStoreEntry(nodeIndex, nodeEntry);
    fsSetNextFree(static_cast<uint16_t>(nextFree + textLen));

    Serial.print(F("Wrote "));
    Serial.print(textLen);
    Serial.print(F(" byte(s) to "));
    Serial.println(path);
    return;
  }

  if (equalsIgnoreCase(argv[1], "rm")) {
    if (argc != 3) {
      Serial.println(F("Usage: fs rm <path>"));
      return;
    }

    uint8_t nodeIndex = kFsRootParent;
    FsEntry nodeEntry;
    if (!fsResolvePath(argv[2], nodeIndex, nodeEntry) || nodeIndex == kFsRootParent) {
      Serial.println(F("Path not found."));
      return;
    }
    if (nodeEntry.isDir && fsHasChildren(nodeIndex)) {
      Serial.println(F("Directory not empty."));
      return;
    }

    fsClearEntry(nodeIndex);
    Serial.print(F("Removed: "));
    Serial.println(argv[2]);
    return;
  }

  if (equalsIgnoreCase(argv[1], "stat")) {
    if (argc != 2) {
      Serial.println(F("Usage: fs stat"));
      return;
    }

    uint8_t used = 0;
    uint8_t dirs = 0;
    uint8_t files = 0;
    for (uint8_t i = 0; i < kFsMaxEntries; ++i) {
      FsEntry entry;
      fsLoadEntry(i, entry);
      if (!entry.used) {
        continue;
      }
      ++used;
      if (entry.isDir) {
        ++dirs;
      } else {
        ++files;
      }
    }

    const size_t total = eepromSize();
    const uint16_t nextFree = fsNextFree();
    const size_t dataCapacity = total - kFsDataStart;
    const size_t dataUsed = nextFree - kFsDataStart;
    const size_t dataFree = total - nextFree;

    Serial.println(F("\n=== FS Stat ==="));
    Serial.print(F("Entries: "));
    Serial.print(used);
    Serial.print(F("/"));
    Serial.println(kFsMaxEntries);
    Serial.print(F("Dirs: "));
    Serial.print(dirs);
    Serial.print(F(", Files: "));
    Serial.println(files);
    Serial.print(F("Data start: 0x"));
    printHexWord(kFsDataStart);
    Serial.print(F(", next free: 0x"));
    printHexWord(nextFree);
    Serial.println();
    Serial.print(F("Data used/free: "));
    Serial.print(dataUsed);
    Serial.print(F("/"));
    Serial.print(dataCapacity);
    Serial.print(F(" bytes (free "));
    Serial.print(dataFree);
    Serial.println(F(")"));
    Serial.println(F("==============\n"));
    return;
  }

  Serial.println(F("Unknown fs command. Use 'fs help'."));
}
#endif

} // namespace shell
