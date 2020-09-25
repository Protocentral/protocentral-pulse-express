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

#define MAX_PPG_BUFF_SZ 500

static uint8_t ppg_buff[MAX_PPG_BUFF_SZ];
static uint16_t buff_counter = 0;
  
ProtocentralMAX32664 pulse_express;

void setup() {

  Wire.begin();
  InitTps65721();
  delay(10);
  
  pulse_express.begin(RAWDATAMODE);
  delay(10);
  

}

void loop() {
  

  uint8_t no_samples = pulse_express.read_raw_samples(ppg_buff);

  delay(7);

}
