//////////////////////////////////////////////////////////////////////////////////////////
//   Basic Arduino program for protocentral pulse express board
//
//   Copyright (c) 2020 ProtoCentral
//
//
//
//
//
//////////////////////////////////////////////////////////////////////////////////////////
#include <Arduino.h>
#include <Wire.h>
#include "protocentral_max32664.h"
#include "protocentral_tps65721.h"


ProtocentralMAX32664 pulse_express;
void setup() {

  Wire.begin();
  InitTps65721();
  delay(10);
  
  pulse_express.begin(RAWDATAMODE);
  delay(10);
  

}

void loop() {
  

}
