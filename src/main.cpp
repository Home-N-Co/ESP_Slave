//
// Created by Kantin FAGNIART on 24/03/2025.
//

#include "Arduino.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Setup terminé !");
}

void loop() {
    Serial.println("Hello World");
    delay(1000);
}