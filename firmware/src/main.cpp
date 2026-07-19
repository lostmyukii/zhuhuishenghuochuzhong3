#include <Arduino.h>

#include "smartlife_config.h"

void setup() {
  Serial.begin(smartlife::SERIAL_BAUD);
}

void loop() {
  delay(1);
}
