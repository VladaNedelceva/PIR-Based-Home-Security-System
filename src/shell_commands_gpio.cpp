#include "shell.hpp"

#include <string.h>

namespace shell {

bool handleGpioCommand(char *argv[], size_t argc) {
  if (argc > 0 && strcmp(argv[0], "pinmode") == 0) {
    if (argc != 3) {
      Serial.println(F("Usage: pinmode <pin> <in|out|pullup>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    if (strcmp(argv[2], "in") == 0 || strcmp(argv[2], "input") == 0) {
      pinMode(pin, INPUT);
      Serial.print(F("pinMode "));
      printPinLabel(pin);
      Serial.println(F(" -> INPUT"));
      return true;
    }
    if (strcmp(argv[2], "out") == 0 || strcmp(argv[2], "output") == 0) {
      pinMode(pin, OUTPUT);
      Serial.print(F("pinMode "));
      printPinLabel(pin);
      Serial.println(F(" -> OUTPUT"));
      return true;
    }
    if (strcmp(argv[2], "pullup") == 0 || strcmp(argv[2], "input_pullup") == 0) {
      pinMode(pin, INPUT_PULLUP);
      Serial.print(F("pinMode "));
      printPinLabel(pin);
      Serial.println(F(" -> INPUT_PULLUP"));
      return true;
    }
    Serial.println(F("Invalid mode. Use in|out|pullup."));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "delay") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: delay <ms>"));
      return true;
    }
    unsigned long delayMs = 0;
    if (!parseUnsignedAuto(argv[1], delayMs) || delayMs > 600000UL) {
      Serial.println(F("Invalid delay. Use 0..600000 ms."));
      return true;
    }
    Serial.print(F("Delaying "));
    Serial.print(delayMs);
    Serial.println(F(" ms..."));
    delay(delayMs);
    Serial.println(F("Done."));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "freq") == 0) {
    if (argc != 2 && argc != 3) {
      Serial.println(F("Usage: freq <pin> [ms]"));
      Serial.print(F("Window: "));
      Serial.print(kMinFreqWindowMs);
      Serial.print(F(".."));
      Serial.print(kMaxFreqWindowMs);
      Serial.println(F(" ms"));
      return true;
    }

    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }

    unsigned long windowMs = kDefaultFreqWindowMs;
    if (argc == 3) {
      if (!parseUnsignedAuto(argv[2], windowMs) || windowMs < kMinFreqWindowMs ||
          windowMs > kMaxFreqWindowMs) {
        Serial.print(F("Invalid window. Use "));
        Serial.print(kMinFreqWindowMs);
        Serial.print(F(".."));
        Serial.print(kMaxFreqWindowMs);
        Serial.println(F(" ms."));
        return true;
      }
    }

    const uint32_t startUs = micros();
    const uint32_t windowUs = static_cast<uint32_t>(windowMs) * 1000UL;
    uint32_t risingEdges = 0;
    int prev = digitalRead(pin);

    while ((uint32_t)(micros() - startUs) < windowUs) {
      const int curr = digitalRead(pin);
      if (prev == LOW && curr == HIGH) {
        ++risingEdges;
      }
      prev = curr;
    }

    const uint32_t elapsedUs = micros() - startUs;
    uint32_t hzWhole = 0;
    uint8_t hzFrac2 = 0;
    if (elapsedUs > 0) {
      const uint64_t hzX100 =
          (static_cast<uint64_t>(risingEdges) * 100000000ULL) / elapsedUs;
      hzWhole = static_cast<uint32_t>(hzX100 / 100ULL);
      hzFrac2 = static_cast<uint8_t>(hzX100 % 100ULL);
    }

    Serial.print(F("freq "));
    printPinLabel(pin);
    Serial.print(F(" ~= "));
    Serial.print(hzWhole);
    Serial.write('.');
    if (hzFrac2 < 10) {
      Serial.write('0');
    }
    Serial.print(hzFrac2);
    Serial.print(F(" Hz (edges="));
    Serial.print(risingEdges);
    Serial.print(F(", window="));
    Serial.print(elapsedUs);
    Serial.println(F(" us)"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "digitalread") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: digitalread <pin>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    const int value = digitalRead(pin);
    printPinLabel(pin);
    Serial.print(F(" = "));
    Serial.print(value ? F("HIGH") : F("LOW"));
    Serial.print(F(" ("));
    Serial.print(value ? 1 : 0);
    Serial.println(F(")"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "digitalwrite") == 0) {
    if (argc != 3) {
      Serial.println(F("Usage: digitalwrite <pin> <0|1>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    unsigned long bit = 0;
    if (!parseUnsigned(argv[2], bit) || bit > 1) {
      Serial.println(F("Invalid value. Use 0 or 1."));
      return true;
    }
    pinMode(pin, OUTPUT);
    digitalWrite(pin, bit ? HIGH : LOW);
    printPinLabel(pin);
    Serial.print(F(" <= "));
    Serial.println(bit ? F("HIGH") : F("LOW"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "analogread") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: analogread <A0-A5>"));
      return true;
    }
    uint8_t analogIndex = 0;
    int pin = -1;
    if (!parseAnalogPinToken(argv[1], analogIndex, pin)) {
      Serial.println(F("Invalid analog pin. Use A0-A5."));
      return true;
    }
    const int value = analogRead(pin);
    Serial.print(F("A"));
    Serial.print(analogIndex);
    Serial.print(F(" = "));
    Serial.println(value);
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "pwm") == 0) {
    if (argc != 3) {
      Serial.println(F("Usage: pwm <pin> <0-255>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    if (!isPwmCapablePin(pin)) {
      Serial.println(F("Pin is not PWM-capable. Use D3,D5,D6,D9,D10,D11."));
      return true;
    }
    unsigned long level = 0;
    if (!parseUnsigned(argv[2], level) || level > 255UL) {
      Serial.println(F("Invalid value. Use 0..255."));
      return true;
    }
    pinMode(pin, OUTPUT);
    analogWrite(pin, static_cast<uint8_t>(level));
    printPinLabel(pin);
    Serial.print(F(" PWM <= "));
    Serial.println(level);
    return true;
  }

#if FEATURE_TONE
  if (argc > 0 && strcmp(argv[0], "tone") == 0) {
    if (argc != 3 && argc != 4) {
      Serial.println(F("Usage: tone <pin> <freq> [ms]"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    unsigned long freq = 0;
    if (!parseUnsigned(argv[2], freq) || freq == 0 || freq > 65535UL) {
      Serial.println(F("Invalid freq. Use 1..65535 Hz."));
      return true;
    }

    if (argc == 4) {
      unsigned long durMs = 0;
      if (!parseUnsigned(argv[3], durMs)) {
        Serial.println(F("Invalid duration ms."));
        return true;
      }
      tone(pin, static_cast<unsigned int>(freq), static_cast<unsigned long>(durMs));
      printPinLabel(pin);
      Serial.print(F(" tone "));
      Serial.print(freq);
      Serial.print(F(" Hz for "));
      Serial.print(durMs);
      Serial.println(F(" ms"));
      return true;
    }

    tone(pin, static_cast<unsigned int>(freq));
    printPinLabel(pin);
    Serial.print(F(" tone "));
    Serial.print(freq);
    Serial.println(F(" Hz"));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "notone") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: notone <pin>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    noTone(pin);
    printPinLabel(pin);
    Serial.println(F(" tone OFF"));
    return true;
  }
#endif

  if (argc > 0 && strcmp(argv[0], "pulse") == 0) {
    if (argc != 5) {
      Serial.println(F("Usage: pulse <pin> <count> <high_ms> <low_ms>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    unsigned long count = 0;
    unsigned long highMs = 0;
    unsigned long lowMs = 0;
    if (!parseUnsigned(argv[2], count) || count == 0) {
      Serial.println(F("Invalid count. Use >= 1."));
      return true;
    }
    if (!parseUnsigned(argv[3], highMs) || !parseUnsigned(argv[4], lowMs)) {
      Serial.println(F("Invalid timing values."));
      return true;
    }

    pinMode(pin, OUTPUT);
    for (unsigned long i = 0; i < count; ++i) {
      digitalWrite(pin, HIGH);
      delay(highMs);
      digitalWrite(pin, LOW);
      if (i + 1 < count) {
        delay(lowMs);
      }
      if (Serial.available() > 0) {
        while (Serial.available() > 0) {
          Serial.read();
        }
        Serial.println(F("Pulse aborted by keypress."));
        return true;
      }
    }
    Serial.println(F("Pulse completed."));
    return true;
  }

  if (argc > 0 && strcmp(argv[0], "watch") == 0) {
    if (argc != 2) {
      Serial.println(F("Usage: watch <pin>"));
      return true;
    }
    int pin = -1;
    if (!parsePinToken(argv[1], pin)) {
      Serial.println(F("Invalid pin. Use D0-D22 or A0-A5."));
      return true;
    }
    while (Serial.available() > 0) {
      Serial.read();
    }

    Serial.print(F("Watching "));
    printPinLabel(pin);
    Serial.println(F(" every 200 ms. Press any key to stop."));
    while (true) {
      const int value = digitalRead(pin);
      printPinLabel(pin);
      Serial.print(F(" = "));
      Serial.print(value ? F("HIGH") : F("LOW"));
      Serial.print(F(" @ "));
      Serial.print(millis());
      Serial.println(F(" ms"));

      const uint32_t start = millis();
      while ((millis() - start) < kWatchPeriodMs) {
        if (Serial.available() > 0) {
          while (Serial.available() > 0) {
            Serial.read();
          }
          Serial.println(F("Watch stopped."));
          return true;
        }
        delay(5);
      }
    }
  }

  return false;
}

} // namespace shell
