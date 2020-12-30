//////////////////////////////////////////////////////////////////////////////////////////
//
//    Demo code for the MAX32664 breakout board
//
//    Author: Joice Tm
//    Copyright (c) 2020 ProtoCentral
//
//    |Max32664 pin label| Arduino Connection  |Pin Function      |
//    |----------------- |---------------------|------------------|
//    | SDA              | A4                  |  Serial Data     |
//    | SCL              | A5                  |  Serial Clock    |
//    | Vin              | 5V                  |  Power           |
//    | GND              | Gnd                 |  Gnd             |
//    | MFIO Pin         | 05                  |  MFIO            |
//    | RESET Pin        | 04                  |  Reset           | 
//    |-----------------------------------------------------------|
//
//    This software is licensed under the MIT License(http://opensource.org/licenses/MIT).
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
//    NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
//    WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
/////////////////////////////////////////////////////////////////////////////////////////


#include <Wire.h>
#include "max32664.h"
#include "Arduino.h"

    uint8_t calibVector[CALIBVECTOR_SIZE] = {0x50, 0x04, 0x03, 0, 0, 175, 63, 3, 33, 75, 0, 0, 0, 0, 15, 198, 2, 100, 3, 32, 0, 0, 3, 207, 0,      //calib vector sample
                                4, 0, 3, 175, 170, 3, 33, 134, 0, 0, 0, 0, 15, 199, 2, 100, 3, 32, 0, 0, 3,
                                207, 0, 4, 0, 3, 176, 22, 3, 33, 165, 0, 0, 0, 0, 15, 200, 2, 100, 3, 32, 0,
                                0, 3, 207, 0, 4, 0, 3, 176, 102, 3, 33, 203, 0, 0, 0, 0, 15, 201, 2, 100, 3,
                                32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 178, 3, 33, 236, 0, 0, 0, 0, 15, 202, 2,
                                100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 255, 3, 34, 16, 0, 0, 0, 0, 15,
                                203, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 64, 3, 34, 41, 0, 0, 0, 0,
                                15, 204, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 130, 3, 34, 76, 0, 0,
                                0, 0, 15, 205, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 189, 3, 34, 90,
                                0, 0, 0, 0, 15, 206, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 248, 3, 34,
                                120, 0, 0, 0, 0, 15, 207, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 178, 69, 3,
                                34, 137, 0, 0, 0, 0, 15, 208, 2, 100, 3, 32, 0, 0, 3, 0, 0, 175, 63, 3, 33,
                                75, 0, 0, 0, 0, 15, 198, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 175, 170, 3,
                                33, 134, 0, 0, 0, 0, 15, 199, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176,
                                22, 3, 33, 165, 0, 0, 0, 0, 15, 200, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3,
                                176, 102, 3, 33, 203, 0, 0, 0, 0, 15, 201, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4,
                                0, 3, 176, 178, 3, 33, 236, 0, 0, 0, 0, 15, 202, 2, 100, 3, 32, 0, 0, 3, 207,
                                0, 4, 0, 3, 176, 255, 3, 34, 16, 0, 0, 0, 0, 15, 203, 2, 100, 3, 32, 0, 0, 3,
                                207, 0, 4, 0, 3, 177, 64, 3, 34, 41, 0, 0, 0, 0, 15, 204, 2, 100, 3, 32, 0, 0,
                                3, 207, 0, 4, 0, 3, 177, 130, 3, 34, 76, 0, 0, 0, 0, 15, 205, 2, 100, 3, 32,
                                0, 0, 3, 207, 0, 4, 0, 3, 177, 189, 3, 34, 90, 0, 0, 0, 0, 15, 206, 2, 100, 3,
                                32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 248, 3, 34, 120, 0, 0, 0, 0, 15, 207, 2,
                                100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 178, 69, 3, 34, 137, 0, 0, 0, 0, 15,
                                208, 2, 100, 3, 32, 0, 0, 3, 0, 0, 175, 63, 3, 33, 75, 0, 0, 0, 0, 15, 198, 2,
                                100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 175, 170, 3, 33, 134, 0, 0, 0, 0, 15,
                                199, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 22, 3, 33, 165, 0, 0, 0, 0,
                                15, 200, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 102, 3, 33, 203, 0, 0,
                                0, 0, 15, 201, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 178, 3, 33, 236,
                                0, 0, 0, 0, 15, 202, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 255, 3, 34,
                                16, 0, 0, 0, 0, 15, 203, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 64, 3,
                                34, 41, 0, 0, 0, 0, 15, 204, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177,
                                130, 3, 34, 76, 0, 0, 0, 0, 15, 205, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3,
                                177, 189, 3, 34, 90, 0, 0, 0, 0, 15, 206, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4,
                                0, 3, 177, 248, 3, 34, 120, 0, 0, 0, 0, 15, 207, 2, 100, 3, 32, 0, 0, 3, 207,
                                0, 4, 0, 3, 178, 69, 3, 34, 137, 0, 0, 0, 0, 15, 208, 2, 100, 3, 32, 0, 0, 3,
                                0, 0, 175, 63, 3, 33, 75, 0, 0, 0, 0, 15, 198, 2, 100, 3, 32, 0, 0, 3, 207, 0,
                                4, 0, 3, 175, 170, 3, 33, 134, 0, 0, 0, 0, 15, 199, 2, 100, 3, 32, 0, 0, 3,
                                207, 0, 4, 0, 3, 176, 22, 3, 33, 165, 0, 0, 0, 0, 15, 200, 2, 100, 3, 32, 0,
                                0, 3, 207, 0, 4, 0, 3, 176, 102, 3};


uint8_t max32664::readSamples( ){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(!ret){
    //Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  uint8_t readBuff[30]={0};
  uint8_t readLen = 23;

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t numSamples = readNumSamples();
  if(numSamples == 0){
    //Serial.println("No samples available");
    return 0;
  }else{
    //Serial.print("num samples ");
   // Serial.println(numSamples);
  }

  for (size_t i = 0; i < numSamples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);
  }

  max32664Output.sys = readBuff[16];
  max32664Output.dia = readBuff[17];
  max32664Output.hr = formHRdata(&readBuff[14]);
  max32664Output.spo2 = formSpo2data(&readBuff[18]);
    
  return (numSamples);
}


uint16_t max32664::formHRdata(uint8_t * hrBuff){

  uint16_t tmp_hr = (uint16_t) hrBuff[0];
  tmp_hr = tmp_hr << 8;

  uint16_t hr = (uint16_t) hrBuff[1];
  hr = (tmp_hr | hr);

  return (hr/10);
}


float max32664::formSpo2data(uint8_t * spo2Buff){
    
  uint16_t tmp = (uint16_t) spo2Buff[0];
  tmp = tmp << 8;

  uint16_t spo2 = (uint16_t) spo2Buff[1];
  spo2 = (tmp | spo2);

  return  ((float)spo2)/10;
}


uint8_t max32664::readCalibSamples(){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(!ret){
    //Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  const uint8_t readLen = 23;
  uint8_t readBuff[readLen]={0};

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t numSamples = readNumSamples();
  if(numSamples == 0){
    //Serial.println("No samples available");
    return false;
  }else{
    //Serial.print("num samples ");
   // Serial.println(numSamples);
  }

  for (size_t i = 0; i < numSamples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);
    delay(10);
  }

  delay(10);
  Serial.print("progress(%) = ");
  Serial.println(readBuff[13]);
  
  return readBuff[12];
}


uint8_t max32664::readRawSamples(int16_t * irBuff, int16_t * redBuff){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(!ret){
    Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  uint8_t readLen = 13;
  uint8_t readBuff[20]={0};

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t numSamples = readNumSamples();
  if(numSamples == 0){
    //Serial.println("No samples available");
    return 0;
  }else{

    if(numSamples > rawDataBuffLen) numSamples = rawDataBuffLen;
    //Serial.print("num samples ");
   // Serial.println(numSamples);
  }

  for (size_t i = 0; i < numSamples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);

    unsigned long ppg0 = (unsigned long ) readBuff[0];//readBuff[1];
    ppg0 = ppg0 << 16;
    unsigned long ppg1 = (unsigned long ) readBuff[1];
    ppg1 = ppg1 << 8;
    unsigned long ppg2 = (unsigned long ) readBuff[2];
    unsigned long unsignedPpg = (unsigned long ) (ppg2 | ppg1 | ppg0);

    int16_t ppgFinal = (int16_t) (unsignedPpg)/10;


    unsigned long red0 = (unsigned long ) readBuff[3];//readBuff[1];
    red0 = red0 << 16;
    unsigned long red1 = (unsigned long ) readBuff[4];
    red1 = red1 << 8;
    unsigned long red2 = (unsigned long ) readBuff[5];
    unsigned long unsignedRed = (unsigned long ) (red2 | red1 | red0);

    int16_t redFinal = (int16_t) (unsignedRed)/10;
   
    irBuff[i] = ppgFinal;
    redBuff[i] = redFinal;
  }

  return (numSamples);
}



uint8_t max32664::readRawSamples(int16_t * irBuff){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(!ret){
    Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  uint8_t readLen = 13;
  uint8_t readBuff[20]={0};

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t numSamples = readNumSamples();
  if(numSamples == 0){
    //Serial.println("No samples available");
    return 0;
  }else{

    if(numSamples > rawDataBuffLen) numSamples = rawDataBuffLen;
    //Serial.print("num samples ");
   // Serial.println(numSamples);
  }

  for (size_t i = 0; i < numSamples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);

    unsigned long ppg0 = (unsigned long ) readBuff[0];//readBuff[1];
    ppg0 = ppg0 << 16;
    unsigned long ppg1 = (unsigned long ) readBuff[1];
    ppg1 = ppg1 << 8;
    unsigned long ppg2 = (unsigned long ) readBuff[2];
    unsigned long unsignedPpg = (unsigned long ) (ppg2 | ppg1 | ppg0);

    int16_t ppgFinal = (int16_t) (unsignedPpg)/10;

    irBuff[i] = ppgFinal;
  }

  return (numSamples);
}


bool max32664::enterAppMode(){

  uint8_t ret = writeByte(0x01, 0x00, 0x00);
  delay(10);
  
  if(!ret){
    //Serial.println("failed to enter app mode !!!");
    return false;
  }
  
  return true;
}


uint8_t max32664::readNumSamples()
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write((uint8_t)0x12);    
  Wire.write((uint8_t)0x00);    

  Wire.endTransmission();
  
  Wire.requestFrom(SENSORHUB_ADDR, 2);
  statusByte = Wire.read();
  if( statusByte ){
    //Serial.println("read num of samples cmd failed");
    return 0;
  }else{
    returnByte = Wire.read(); 
    return returnByte;
  }
}


bool max32664::readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t * readBuff, uint8_t readLen){
  
  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission();
 // delay(DELAY_CMD);
  
  Wire.requestFrom(SENSORHUB_ADDR, readLen+1);
  statusByte = Wire.read();
  
  if( statusByte ){
    
    return false;
  }else{

    for(int i=0; i<readLen; i++){
      
      readBuff[i] = Wire.read();
      delayMicroseconds(10);
    }

    return true;;
  }
}


bool max32664::readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t * readBuff, uint8_t readLen){
  
  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3);    
  Wire.endTransmission();
 // delay(DELAY_CMD);
  
  Wire.requestFrom(SENSORHUB_ADDR, readLen+1);
  statusByte = Wire.read();
  
  if( statusByte ){
    
    return false;
  }else{

    for(int i=0; i<readLen; i++){
      
      readBuff[i] = Wire.read();
      delayMicroseconds(10);
    }

    return true;;
  }
}


bool max32664::writeMultipleBytes(uint8_t * wrBuff, uint8_t wrLen){
  
  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);

  for(int i=0; i<wrLen; i++){
    
    Wire.write(wrBuff[i]);
    delayMicroseconds(10);
  }
        
  Wire.endTransmission();
  delay(6);   //
  
  Wire.requestFrom(SENSORHUB_ADDR, 1);
  statusByte = Wire.read();
  
  if( statusByte ){
    //Serial.println("writeMultipleBytes cmd failed");
    return false;
  }else{
    return true;;
  }
}


void max32664::loadAlgorithmParameters(algomodeInitialiser * algoParameters){

  memcpy(calibValSys, &algoParameters->calibValSys[0], 3);
  memcpy(calibValDia, &algoParameters->calibValDia[0], 3);

  spo2CalibCoefA = algoParameters->spo2CalibCoefA;
  spo2CalibCoefB = algoParameters->spo2CalibCoefB;
  spo2CalibCoefC = algoParameters->spo2CalibCoefC;
}


bool max32664::loadBPTcalibrationVector(){
  
  calibVector[0] = 0x50;
  calibVector[1] = 0x04;
  calibVector[2] = 0x03;

  bool ret;
  uint8_t maxWrSize = 30;
  uint8_t wrLoopCnt = (CALIBVECTOR_SIZE / maxWrSize);
  uint8_t wrOffset = (CALIBVECTOR_SIZE % maxWrSize);
  int i = 0;
  
  for( i=0; i<maxWrSize; i++){
    
    Wire.beginTransmission(SENSORHUB_ADDR);

    for(int j=0; j<maxWrSize; j++){
      
      Wire.write(calibVector[(i*maxWrSize) + j]);
      delayMicroseconds(1);
    }
        
    Wire.endTransmission(true);
    delay(6);   //
  }

  ret = writeMultipleBytes(&calibVector[(i*maxWrSize)], wrOffset);
  delay(2);
  
  return true;
  //return ret;
}


bool max32664::setDateTime(){
  
  uint8_t wrBuffer[14]= {0x50, 0x04, 0x04, 0x5c, 0xc2, 0x02, 0x00, 0xe0, 0x7f, 0x02, 0x00};
  bool ret = writeMultipleBytes(wrBuffer, 11);

  return ret;
}


bool  max32664::loadSpo2Coefficients(){

  uint32_t A = spo2CalibCoefA * 100000 ;
  uint32_t B = spo2CalibCoefB * 100000 ;
  uint32_t C = spo2CalibCoefC * 100000 ;

  //uint8_t wrBuffer[17]= {0x50, 0x04, 0x06, 0x00, 0x02, 0x6f, 0x60, 0xff, 0xcb, 0x1d, 0x12, 0x00, 0xab, 0xf3, 0x7b};
  uint8_t wrBuffer[17] = {0x50, 0x04, 0x06};
  wrBuffer[3] = (A & 0xff000000) >> 24;
  wrBuffer[4] = (A & 0x00ff0000) >> 16;
  wrBuffer[5] = (A & 0x0000ff00) >> 8;
  wrBuffer[6] = (A & 0x000000ff);

  wrBuffer[7] = (B & 0xff000000) >> 24;
  wrBuffer[8] = (B & 0x00ff0000) >> 16;
  wrBuffer[9] = (B & 0x0000ff00) >> 8;
  wrBuffer[10] = (B & 0x000000ff);

  wrBuffer[11] = (C & 0xff000000) >> 24;
  wrBuffer[12] = (C & 0x00ff0000) >> 16;
  wrBuffer[13] = (C & 0x0000ff00) >> 8;
  wrBuffer[14] = (C & 0x000000ff);

  bool ret = writeMultipleBytes(wrBuffer, 15);

  return ret;
}


bool  max32664::loadSysCalibrationValues(){

  uint8_t wrBuffer[6]= {0x50, 0x04, 0x01};
  memcpy(&wrBuffer[3], calibValSys, 3);

  bool ret = writeMultipleBytes(wrBuffer, 6);

  return ret;
}


bool  max32664::loadDiastolicCalibrationValues(){

  uint8_t wrBuffer[6]= {0x50, 0x04, 0x02};
  memcpy(&wrBuffer[3], calibValDia, 3);

  bool ret = writeMultipleBytes(wrBuffer, 6);

  return ret;  
}


bool max32664::configRawdataMode(){

  //enter app mode
  bool ret = writeByte(0x01, 0x00, 0x00);
  delay(10);
  if(!ret){
    Serial.println("enter app mode cmd failed");
    return false;
  }

  //Set output mode to sensor data 
  ret = writeByte(0x10, 0x00, 0x01);
  delay(10);
  if(!ret){
    Serial.println("Set output mode to sensor data cmd failed");
    return false;
  }

  //Set sensor hub interrupt threshold
  ret = writeByte(0x10, 0x01, 0x0f);
  delay(10);
  if(!ret){
    Serial.println("Set interrupt threshold cmd failed");
    return false;
  }
  
  //Enable AFE
  ret = writeByte(0x44, 0x03, 0x01);
  delay(10);
  if(!ret){
    Serial.println("Enable AFE cmd failed");
    return false;
  }

  //Enable BPT algorithm in Estimation mode
  ret = writeByte(0x52, 0x04, 0x02);
  delay(10);
  if(!ret){
    Serial.println("Enable AFE cmd failed");
    return false;
  }

  //Disable AGC.
  ret = writeByte(0x52, 0x00, 0x00);
  delay(200);
  if(!ret){
    Serial.println("Disable AGC cmd failed");
    return false;
  }

  //led1
  ret = writeByte(0x40, 0x03, 0x0c, 0x7f);
  delay(10);
  if(!ret){
     Serial.println("LED1 (red) current set failed");
    return false;
  }
  
  //led2
  ret = writeByte(0x40, 0x03, 0x0d, 0x7f);
  delay(10);
  if(!ret){
    Serial.println("LED2 (IR) current set failed");
    return false;
  }

  Serial.println("Device configured in rawdata mode");
  return true;
}


bool max32664::configAlgoInEstimationMode(){

  //load BPT calibration vector
  bool ret = loadBPTcalibrationVector();
  if(!ret) return false;
  delay(1000);

  //set date, time
  ret = setDateTime();
  if(!ret) return false;
  delay(30);

  ret = loadSpo2Coefficients();
  if(!ret) return false;
  delay(30);

  //enable algorithm in ppg and algo mode
  ret = writeByte(0x10, 0x00, 0x03);
  if(!ret) return false;
  delay(30);

  //set intr th
  ret = writeByte(0x10, 0x01, 0x0f);
  if(!ret) return false;
  delay(30);

  //enable AGC
  ret = writeByte(0x52, 0x00, 0x01);
  if(!ret) return false;
  delay(30);

  //enable afe
  ret = writeByte(0x44, 0x03, 0x01);
  if(!ret) return false;
  delay(30);

  //enable BPT algo in estimation mode
  ret = writeByte(0x52, 0x04, 0x02);
  if(!ret) return false;
  delay(120);

  //Serial.println("Device configured in algorithm mode");
  
  return true;
}


bool max32664::startBPTcalibration(){

 // Serial.println("configuring calibration mode");
  enterAppMode();
  
  //set date, time
  bool cmdStatus = setDateTime();
  if(!cmdStatus){
    //Serial.println("failed setdate cmd");
  }
  delay(30);
  
  cmdStatus =loadSysCalibrationValues();
  if(!cmdStatus){
    //Serial.println("failed loadSysCalibrationValues");
    return false;
  }
  delay(30);
  
  cmdStatus =loadDiastolicCalibrationValues();
  if(!cmdStatus){
   // Serial.println("failed loadDiastolicCalibrationValues");
    return false;
  }
  delay(30);
  
  //enable algorithm in ppg and algo mode
  cmdStatus =writeByte(0x10, 0x00, 0x03);
  if(!cmdStatus){
    //Serial.println("failed to enable algorithm");
    return false;
  }
  delay(30);
  
  
//set intr th
  cmdStatus =writeByte(0x10, 0x01, 0x0f);
  if(!cmdStatus){

    return false;
  }
  delay(30);
  
  //enable afe
  cmdStatus =writeByte(0x44, 0x03, 0x01);
  if(!cmdStatus){
   // Serial.println("failed to enable afe");

    return false;
  }
  delay(30);
  
  //enable BPT algo in calibration mode
  cmdStatus =writeByte(0x52, 0x04, 0x01);
  if(!cmdStatus){
    //Serial.println("failed to enable BPT algo");
    return false;
  }
  
  Serial.println("Please keep your finger on sensor untill progress reach 100%");
  delay(120);

  uint8_t bpStatus = readCalibSamples();

  while(bpStatus != 2 && (max32664Output.progress != 100) ){ //bpStatus, 0x02 == success

    bpStatus = readCalibSamples();
    delay(1000);

    if(bpStatus == 05){
      
     // Serial.println("calibration failed");
      cmdStatus = false;
      break;
    }
  }

  //Serial.println("completed calibration");
  if (cmdStatus){
    //readCalibrationVector();
    delay(1000);
  }
  //delay(1000);
  
  return cmdStatus;
}


bool  max32664::setFifoIntrThreshold(uint8_t threshold){
  
  uint8_t ret = writeByte(0x10, 0x01, threshold);
  delay(10);
  if(!ret){
   // Serial.println("disable afe failed");
    return false;
  }else return true;
}


//write values to max301xx registes
bool  max32664::writeMax301xxReg(uint8_t regAddr, uint8_t val){
  
  uint8_t ret = writeByte(0x40, 0x02, regAddr, val);
  delay(10);
  if(!ret){
    return false;
  }else return true;
}


//Automatic Gain Control (AGC) algorithm: Set the target percentage of the full-scale ADC
//range that the automatic gain control (AGC) algorithm uses. 
bool max32664::SetTargetPercentagefullFcaleADC(uint8_t percent){

  uint8_t ret = writeByte(0x50, 0x00, 0x00, percent);
  delay(10);
  if(!ret){
    return false;
  }else return true;
}



uint8_t  max32664::readTargetPercentagefullFcaleADC(void){
  
  uint8_t ret = readByte(0x51, 0x00, 0x00);
  delay(10);
  if(!ret){
    return ERR_UNKNOWN;
  }else return ret;
}


bool max32664::SetAGCalgoStepSize(uint8_t percent){

  uint8_t ret = writeByte(0x50, 0x00, 0x01, percent);
  delay(10);
  if(!ret){
    return false;
  }else return true;
}


uint8_t  max32664::readAGCalgoStepSize(void){
  
  uint8_t ret = readByte(0x51, 0x00, 0x01);
  delay(10);
  if(!ret){
    return ERR_UNKNOWN;
  }else return ret;
}


bool max32664::SetAGCalgoSensitivity(uint8_t percent){

  uint8_t ret = writeByte(0x50, 0x00, 0x02, percent);
  delay(10);
  if(!ret){
    return false;
  }else return true;
}


uint8_t  max32664::readAGCalgoSensitivity(void){
  
  uint8_t ret = readByte(0x51, 0x00, 0x02);
  delay(10);
  if(!ret){
    return ERR_UNKNOWN;
  }else return ret;
}



bool max32664::SetAGCalgoNumSamplestoAvg(uint8_t numSamples){

  uint8_t ret = writeByte(0x50, 0x00, 0x03  , numSamples);
  delay(10);
  if(!ret){
    return false;
  }else return true;
}


uint8_t  max32664::readAGCalgoNumSamplestoAvg(void){
  
  uint8_t ret = readByte(0x51, 0x00, 0x03);
  delay(10);
  if(!ret){
    return ERR_UNKNOWN;
  }else return ret;
}


uint8_t  max32664::readMax301xxReg(uint8_t regAddr){
  
  uint8_t ret = readByte(0x40, 0x02, regAddr);
  delay(10);
  if(!ret){
    return ERR_UNKNOWN;
  }else return ret;
}


void  max32664::readCalibrationVector(){

//Disable AFE
  uint8_t ret = writeByte(0x44, 0x03, 0x00);
  delay(10);
  if(!ret){
   // Serial.println("disable afe failed");
    return;
  }
  
  //Disable BPT algorithm
  ret = writeByte(0x52, 0x04, 0x00);
  delay(10);
  if(!ret){
   // Serial.println("failed to set raw data mode !!!");
    return;
  }

  //readMultipleBytes(0x51, 0x04, 0x00, &calibVector[3], 823);
  readMultipleBytes(0x02, 0x00, &calibVector[3], 824);

#if 0   
//debug, print the read vector.
  for(int i =0; i<CALIBVECTOR_SIZE; i++){
    Serial.println(calibVector[i]);
    delayMicroseconds(10);
  }
#endif
  delay(1000);  
}


uint8_t max32664::hubBegin(){

  pinMode(mfioPin, OUTPUT); 
  pinMode(ResetPin, OUTPUT);

  digitalWrite(mfioPin, HIGH); 
  digitalWrite(ResetPin, LOW); 
  delay(10); 
  digitalWrite(ResetPin, HIGH); 
  delay(1000); 
  pinMode(mfioPin, INPUT_PULLUP); // To be used as an interrupt later

  readSensorHubVersion();
  uint8_t responseByte = readByte(READ_DEVICE_MODE, 0x00); // 0x00 only possible Index Byte.
  
  return responseByte; 
}


bool max32664::readSensorHubVersion(){
  
  uint8_t returnByte;
  uint8_t statusByte;
  uint8_t readBuff[4];

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(0xff);    
  Wire.write(0x03);    
  Wire.endTransmission();
  
  Wire.requestFrom(SENSORHUB_ADDR, 4);
  statusByte = Wire.read();
  
  if(statusByte){
    
    //Serial.println("read algo version failed");
    return false;
  }else{

    Serial.print("Hub Version  ");
    for(int i=0; i<3; i++){
      readBuff[i] = Wire.read();
      delayMicroseconds(10);
      Serial.print(readBuff[i]);
      Serial.print(".");
    }
    Serial.println(" ");

    return true;
  }
}


bool max32664::readSensorHubAlgoVersion(void){
  
  uint8_t returnByte;
  uint8_t statusByte;
  uint8_t readBuff[4];

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(0xff);    
  Wire.write(0x07);    
  Wire.endTransmission();
  
  Wire.requestFrom(SENSORHUB_ADDR, 4);
  statusByte = Wire.read();
  
  if(statusByte){
    
    //Serial.println("read algo version failed");
    return false;
  }else{

    Serial.print("algo Version  ");
    for(int i=0; i<3; i++){
      readBuff[i] = Wire.read();
      delayMicroseconds(10);
      Serial.print(readBuff[i]);
      Serial.print(".");
    }
    Serial.println(" ");

    return true;
  }
}


uint8_t max32664::readByte(uint8_t data1, uint8_t data2 )
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission();
  delay(DELAY_CMD);
  
  Wire.requestFrom(SENSORHUB_ADDR, 2); 
  statusByte = Wire.read();
  if( statusByte )
    return statusByte; // sensor returns error, check error code for more info.

  returnByte = Wire.read(); 
  return returnByte; 

}


uint8_t max32664::readByte(uint8_t data1, uint8_t data2, uint8_t data3 )
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3);    
  Wire.endTransmission();
  delay(DELAY_CMD);
  
  Wire.requestFrom(SENSORHUB_ADDR, 2); 
  statusByte = Wire.read();
  if( statusByte )
    return statusByte; // sensor returns error, check error code for more info.

  returnByte = Wire.read(); 
  return returnByte; 

}


bool max32664::writeByte(uint8_t data1, uint8_t data2)
{
  Wire.beginTransmission(SENSORHUB_ADDR);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(SENSORHUB_ADDR, 1); 
  uint8_t statusByte = Wire.read(); 
  
  if(statusByte)
    return false;
  else
    return true; 
}


bool max32664::writeByte(uint8_t data1, uint8_t data2, uint8_t data3)
{
  Wire.beginTransmission(SENSORHUB_ADDR);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3); 
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(SENSORHUB_ADDR, 1); 
  uint8_t statusByte = Wire.read(); 
  
  if(statusByte)
    return false;
  else
    return true; 
}


bool max32664::writeByte(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4)
{
  Wire.beginTransmission(SENSORHUB_ADDR);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3); 
  Wire.write(data4);
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(SENSORHUB_ADDR, 1); 
  uint8_t statusByte = Wire.read(); 

  if(statusByte)
    return false;
  else
    return true; 
}