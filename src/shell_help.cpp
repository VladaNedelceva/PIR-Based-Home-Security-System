#include "shell.hpp"

namespace shell {

void printHelp() {
  Serial.println(F("\n=== Help ==="));
  Serial.println(F("Shell:"));
  Serial.println(F("  help                - show this help"));
  Serial.println(F("  status              - show shell status"));
  Serial.println(F("  ver                 - firmware/build info"));
  Serial.println(F("  id                  - board + MCU signature"));
  Serial.println(F("  echo <text>         - echo text back"));
  Serial.println(F("  reset               - watchdog software reset"));
  Serial.println(F("  free                - free RAM estimate"));
  Serial.println(F("  uptime              - formatted uptime"));

  Serial.println(F("Timing:"));
  Serial.println(F("  micros              - current micros()"));
  Serial.println(F("  delay <ms>          - blocking delay"));
  Serial.println(F("  freq <pin> [ms]     - estimate input frequency"));

  Serial.println(F("GPIO:"));
  Serial.println(F("  pinmode <pin> <in|out|pullup>"));
  Serial.println(F("  digitalread <pin>"));
  Serial.println(F("  digitalwrite <pin> <0|1>"));
  Serial.println(F("  analogread <A0-A5>"));
  Serial.println(F("  pwm <pin> <0-255>"));
#if FEATURE_TONE
  Serial.println(F("  tone <pin> <freq> [ms]"));
  Serial.println(F("  notone <pin>"));
#endif
  Serial.println(F("  pulse <pin> <count> <high_ms> <low_ms>"));
  Serial.println(F("  watch <pin>         - press any key to stop"));

#if FEATURE_I2C
  Serial.println(F("I2C:"));
  Serial.println(F("  i2cscan             - scan I2C bus"));
  Serial.println(F("  i2cspeed <100k|400k> - set bus speed"));
  Serial.println(F("  i2cread <addr> <n>  - read N bytes"));
  Serial.println(F("  i2cwrite <addr> <bytes...>"));
  Serial.println(F("  i2cwr <addr> <reg> <bytes...>"));
  Serial.println(F("  i2crr <addr> <reg> <n>"));
#endif

#if FEATURE_EEPROM
  Serial.println(F("EEPROM:"));
  Serial.println(F("  eepread <addr> [len]"));
  Serial.println(F("  eepwrite <addr> <bytes...>"));
  Serial.println(F("  eeperase confirm    - clear EEPROM"));
#endif

#if FEATURE_FS
  Serial.println(F("FS (EEPROM):"));
  Serial.println(F("  fs help             - filesystem commands"));
#endif

#if FEATURE_LOWLEVEL
  Serial.println(F("Low-level AVR:"));
  Serial.println(F("  ddr <port> [value]  - view/set DDRx"));
  Serial.println(F("  port <port> [value] - view/set PORTx"));
  Serial.println(F("  pin <port>          - read PINx"));
  Serial.println(F("  peek <addr>         - read memory byte"));
  Serial.println(F("  poke <addr> <val>   - write memory byte"));
  Serial.println(F("  reg                 - dump AVR core registers"));
#endif
  Serial.println();
}

void printStatus() {
  Serial.println(F("\n=== Board Status ==="));
  Serial.print(F("Uptime [ms]: "));
  const uint32_t upMs = millis();
  Serial.print(upMs);
  Serial.print(F(" ("));
  printUptimeFormatted(upMs);
  Serial.println(F(")"));
  Serial.print(F("Free RAM [bytes]: "));
  Serial.println(freeRamEstimate());
  Serial.println(F("============================\n"));
}

} // namespace shell
