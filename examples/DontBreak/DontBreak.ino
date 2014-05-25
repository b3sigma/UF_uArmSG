#include <Servo.h>
#include <EEPROM.h>
#include <UF_uArmSG.h>

UF_uArm uarm;

void setup() {
  uarm.init();
}

void loop() {
  uarm.calibration();
  uarm.notify(50);
  delay(500);
}
