#include <Arduino.h>
#include <Wire.h>
#include "protocentral_max32664.h"

bool ProtocentralMAX32664::begin(uint8_t mode)
{
    bool ret = false;
    pinMode(MAX32664_RESET, OUTPUT);
    pinMode(MAX32664_MFIO, OUTPUT);

    selectMax32664AppMode();

    switch (mode) {
      case ALGOMODE:
              //select application mode.
              ret = config_algo_mode();
        break;

      case RAWDATAMODE:
              //configure the sensor in raw data mode
              ret = configRawdataMode();
        break;

      default :
          break;
    }

    return ret;
}

bool ProtocentralMAX32664::config_algo_mode(){

    return true;
}

bool ProtocentralMAX32664::read_sampledata_max32664(uint8_t * writeBuff, uint8_t wr_len, uint8_t * readBuff, uint8_t rd_len){

    Wire.beginTransmission(MAX32664_ADDR);   // Initialize the Tx buffer
    for (size_t i = 0; i < wr_len; i++) {
        Wire.write(writeBuff[i]);
    }
    Wire.endTransmission(false);

    Wire.requestFrom(MAX32664_ADDR, rd_len);
    for (size_t i = 0; i < rd_len; i++) {
        readBuff[0] = Wire.read();
    }

    return true;
}

bool ProtocentralMAX32664::read_data_max32664(uint8_t * wrBuffer, uint16_t len, uint8_t * rdBuffer){

    Wire.beginTransmission(MAX32664_ADDR);   // Initialize the Tx buffer
    for (size_t i = 0; i < len; i++) {
        Wire.write(wrBuffer[i]);
    }
    Wire.endTransmission(false);

    Wire.requestFrom(MAX32664_ADDR, (uint8_t) 2);
    rdBuffer[0] = Wire.read();
    rdBuffer[1] = Wire.read();

    if(rdBuffer[0]){
       return false;
    }

    return true;
}

uint8_t ProtocentralMAX32664::read_raw_samples(uint8_t * ppg_buff){

    uint8_t wr_data[2] = {0, 0};
    if((sendCommand(wr_data, 2))!= true){
        return false;
    }

    //read no of samples
    uint8_t wr[4] = {0x12, 0x00};
    uint8_t read_buff[2000]={0};
    read_data_max32664(wr, 2, read_buff);

    uint8_t no_samples = read_buff[1];
    if(no_samples == 0){
        return 0;
        printf("----- no samples\n" );
    }

    wr[0]= 0x12;
    wr[1]= 0x01;

    for (size_t i = 0; i < no_samples; i++) {

      read_sampledata_max32664(wr, 2, read_buff, 13);

      unsigned long ppg0 = (unsigned long ) read_buff[1];
      ppg0 = ppg0 << 24;
      unsigned long ppg1 = (unsigned long ) read_buff[2];
      ppg1 = ppg1 << 16;
      unsigned long ppg2 = (unsigned long ) read_buff[3];
      ppg2 = ppg2 << 8;
      unsigned long unsigned_ppg = (unsigned long ) (ppg2 | ppg1 | ppg0);

      signed long signed_ppg = (signed long) unsigned_ppg;
      signed_ppg =  signed_ppg >> 8;
      int16_t ppg_final = (int16_t) (signed_ppg)/10;

      uint16_t index = (2*i);
      ppg_buff[index] = (uint8_t)ppg_final;
      ppg_buff[index+1] = (uint8_t)  (ppg_final>>8);
    }

    return (no_samples*2);
}

bool ProtocentralMAX32664::configRawdataMode(){

    bool ret_flag = true;

    uint8_t wr[5]={0x10, 0x00, 0x01};
    bool ret = sendCommand(wr, 3);
    ret_flag &= ret;
    delay(10);

    wr[0]=0x10;
    wr[1]= 0x01;
    wr[2]= 0x0f;

    ret = sendCommand(wr, 3);
    delay(10);
    ret_flag &= ret;

    wr[0]=0x44;
    wr[1]= 0x03;
    wr[2]= 0x01;

    ret = sendCommand(wr, 3);
    delay(10);
    ret_flag &= ret;

    wr[0]=0x52;
    wr[1]= 0x04;
    wr[2]= 0x02;

    ret = sendCommand(wr, 3);
    delay(10);
    ret_flag &= ret;

    wr[0]=0x52;
    wr[1]= 0x00;
    wr[2]= 0x00;

    ret = sendCommand(wr, 3);
    delay(200);
    ret_flag &= ret;

    wr[0]=0x40;
    wr[1]= 0x03;
    wr[2]= 0x0c;
    wr[3]= 0x7f;

    ret = sendCommand(wr, 4);
    delay(10);
    ret_flag &= ret;

    wr[0]=0x40;
    wr[1]= 0x03;
    wr[2]= 0x0d;
    wr[3]= 0x7f;

    ret = sendCommand(wr, 4);
    delay(10);
    ret_flag &= ret;

    return ret;
}

void ProtocentralMAX32664::selectMax32664AppMode(){

  digitalWrite(MAX32664_RESET, 0);
  delay(10);

  digitalWrite(MAX32664_RESET, 1);
  digitalWrite(MAX32664_MFIO, 1);
  delay(1000);

  //debug
  uint8_t wr_buff[4]={0x01, 0x00, 0x00};
  sendCommand(wr_buff, 3);

  //uint8_t wr_buf[4]={READ_DEVICE_MODE, 0x00, 0,0};
//  sendCommand(wr_buf, 2);
}

bool ProtocentralMAX32664::sendCommand(uint8_t * write_buffer, uint8_t len){

    uint8_t read_buff[2] = {0xff};

    I2CwriteBytes(MAX32664_ADDR, write_buffer, len);
    delay(45);

    if(ReadStatus() == 0){
        return true;
    }else{
        return false;
    }
}

uint8_t ProtocentralMAX32664::ReadStatus(){
    // Status Byte, 0x00 is a successful transmit.
    Wire.requestFrom(MAX32664_ADDR, (uint8_t) 1);
    uint8_t ret = Wire.read();
    delay(10);

    return ret;
}

void ProtocentralMAX32664::I2CwriteByte(uint8_t address, uint8_t subAddress, uint8_t data){
  	Wire.beginTransmission(address);  // Initialize the Tx buffer
  	Wire.write(subAddress);           // Put slave register address in Tx buffer
  	Wire.write(data);                 // Put data in Tx buffer
  	Wire.endTransmission();           // Send the Tx buffer
}

//not tested
void ProtocentralMAX32664::I2CwriteBytes(uint8_t address,  uint8_t * data, uint8_t len){
  	Wire.beginTransmission(address);

    for (size_t i = 0; i < len; i++) {
        Wire.write(data[i]);
    }

  	Wire.endTransmission();
}

uint8_t ProtocentralMAX32664::I2CreadByte(uint8_t address, uint8_t subAddress){
  	uint8_t data; // `data` will store the register data
  	Wire.beginTransmission(address);
  	Wire.write(subAddress);
  	Wire.endTransmission(false);
  	Wire.requestFrom(address, (uint8_t) 1);
  	data = Wire.read();
  	return data;
}

void ProtocentralMAX32664::I2CreadBytes(uint8_t address, uint8_t subAddress, uint8_t * dest, uint8_t count){
  	Wire.beginTransmission(address);   // Initialize the Tx buffer
  	Wire.write(subAddress);
  	Wire.endTransmission(false);
  	uint8_t i = 0;
  	Wire.requestFrom(address, count);  // Read bytes from slave register address
  	while (Wire.available())
  	{
  		dest[i++] = Wire.read();
  	}
}
