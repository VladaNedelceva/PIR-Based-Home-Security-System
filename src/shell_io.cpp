#include "shell.hpp"

#include <ctype.h>

namespace shell {

void updateSerial() {
  while (Serial.available() > 0) {
    const char c = static_cast<char>(Serial.read());

    if (gEscState == EscState::SeenEsc) {
      gEscState = (c == '[') ? EscState::SeenEscBracket : EscState::None;
      continue;
    }

    if (gEscState == EscState::SeenEscBracket) {
      if (c == 'A') {
        historyUp();
      } else if (c == 'B') {
        historyDown();
      }
      gEscState = EscState::None;
      continue;
    }

    if (c == 0x1B) {
      gEscState = EscState::SeenEsc;
      continue;
    }

    if (c == '\r') {
      continue;
    }

    if (c == '\b' || c == 127) {
      if (gCmdLen > 0) {
        --gCmdLen;
        Serial.print(F("\b \b"));
      }
      continue;
    }

    if (c == '\n') {
      Serial.println();
      gCmdBuffer[gCmdLen] = '\0';
      pushHistory(gCmdBuffer);
      handleCommand(gCmdBuffer);
      gCmdLen = 0;
      resetHistoryBrowse();
      printPrompt();
      continue;
    }

    if (isprint(static_cast<unsigned char>(c)) && gCmdLen < (kCmdBufferSize - 1)) {
      gCmdBuffer[gCmdLen++] = c;
      Serial.write(c); // Echo typed input.
    }
  }
}


} // namespace shell
