#include <Arduino.h>
#include <Wire.h>
#include "protocentral_tps65721.h"

volatile bool tps65721_intr_flag = false;


bool InitTps65721(void){
    //tps led
  //  tps65721_write(TPS65721_ADDR, 0x0b,0x04);
  //  tps65721_write(TPS65721_ADDR, 0x0c,0xf3);
  //  tps65721_write(TPS65721_ADDR, 0x0f,0xef);

    tps65721_write(TPS65721_ADDR, CHGCONFIG0, 0xaf );
    tps65721_write(TPS65721_ADDR, CHGCONFIG1, 0x77 );
    tps65721_write(TPS65721_ADDR, CHGCONFIG2, 0x4c );
    tps65721_write(TPS65721_ADDR, CHGCONFIG3, 0x41 );
}

void tps65721_intr_handler()
{
    Serial.println("tps65721_intr_handler");
    tps65721_read(0x48, 0x10);
    uint8_t data = tps65721_read(0x48, 0x01);
    data = (data & 0x0c);
    tps65721_intr_flag = false;
    if(data)
    {
      data = (data & 0x04);   
    }
     else
    {
    }
}

bool check_charging_status(void){

    uint8_t data = tps65721_read(0x48, 0x01);

    data = (data & 0x0c);
    if(data) data = (data & 0x04);
    if(data)
    {
      turn_off_leds_tps();
      digitalWrite(LED_RED, LOW);
      return true;
    }else{
      digitalWrite(LED_RED, HIGH);
      set_blue_led_tps();
      return false;
    }
}


void tps65721_write(uint8_t address, uint8_t subAddress, uint8_t data)
{
    Wire.beginTransmission(address);  // Initialize the Tx buffer
    Wire.write(subAddress);           // Put slave register address in Tx buffer
    Wire.write(data);                 // Put data in Tx buffer
    Wire.endTransmission();           // Send the Tx buffer
}

uint8_t tps65721_read(uint8_t address, uint8_t subAddress)
{
    uint8_t data;
    Wire.beginTransmission(address);
    Wire.write(subAddress);
    Wire.endTransmission(false);
    Wire.requestFrom(address, (uint8_t) 1);
    data = Wire.read();
    return data;
}

void turn_off_leds_tps(void){

   tps65721_write(0x48, 0x0b, 0xff);
}

void set_green_led_tps(void){

   tps65721_write(0x48, 0x0b, 0x08);
}

void set_blue_led_tps(void){

   tps65721_write(0x48, 0x0b, 0x04);
}
