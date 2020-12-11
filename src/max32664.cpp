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
//    For information on how to use, visit https://github.com/Protocentral/protocentral-max30003-arduino
//
/////////////////////////////////////////////////////////////////////////////////////////


#include <Wire.h>
#include "max32664.h"
uint8_t max32664::readSamples( ){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(!ret){
    //Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  uint8_t readBuff[30]={0};
  uint8_t readLen = 23;

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t num_samples = readNumSamples();
  if(num_samples == 0){
    //Serial.println("No samples available");
    return 0;
  }else{
    //Serial.print("num samples ");
   // Serial.println(num_samples);
  }

  for (size_t i = 0; i < num_samples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);
  }

  max32664Output.sys = readBuff[16];
  max32664Output.dia = readBuff[17];
  max32664Output.hr = formHRdata(&readBuff[14]);
  max32664Output.spo2 = formSpo2data(&readBuff[18]);
    
  return (num_samples);
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
    Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  const uint8_t readLen = 23;
  uint8_t readBuff[readLen]={0};

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t num_samples = readNumSamples();
  if(num_samples == 0){
    Serial.println("No samples available");
    return false;
  }else{
    //Serial.print("num samples ");
   // Serial.println(num_samples);
  }

  for (size_t i = 0; i < num_samples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);
    delay(10);
  }

  delay(10);
  Serial.print("progress(%) = ");
  Serial.println(readBuff[13]);
  
  return readBuff[12];
}


bool max32664::enterAppMode(){

  uint8_t ret = writeByte(0x01, 0x00, 0x00);
  delay(10);
  
  if(!ret){
    Serial.println("failed to enter app mode !!!");
    return false;
  }
  
  return true;
}

uint8_t max32664::readNumSamples()
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(SENSORHUB_ADDR);
  Wire.write(0x12);    
  Wire.write(0x00);    
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
    
    Serial.println("fifo sample read cmd failed");
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
  
  uint8_t wrBuffer[20]= {0x50, 0x04, 0x04, 0x5c, 0xc2, 0x02, 0x00, 0xe0, 0x7f, 0x02, 0x00};
  bool ret = writeMultipleBytes(wrBuffer, 11);
  return ret;
}


bool  max32664::loadSpo2Coefficients(){
  
  uint8_t wrBuffer[20]= {0x50, 0x04, 0x06, 0x00, 0x02, 0x6f, 0x60, 0xff, 0xcb, 0x1d, 0x12, 0x00, 0xab, 0xf3, 0x7b};
  bool ret = writeMultipleBytes(wrBuffer, 15);

  return ret;
}


bool  max32664::loadSysCalibrationValues(){

  uint8_t wrBuffer[20]= {0x50, 0x04, 0x01, 0x78, 0x7a, 0x7d};
  bool ret = writeMultipleBytes(wrBuffer, 6);

  return ret;
}

bool  max32664::loadDiastolicCalibrationValues(){

  uint8_t wrBuffer[20]= {0x50, 0x04, 0x02, 0x50, 0x51, 0x52};
  bool ret = writeMultipleBytes(wrBuffer, 6);

  return ret;  
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

  Serial.println("Device configured in algorithm mode");
  
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
   // Serial.println("failed to enable algorithm");
    return false;
  }
  delay(30);
  
  //enable afe
  cmdStatus =writeByte(0x44, 0x03, 0x01);
  if(!cmdStatus){
   // Serial.println("failed to enable algorithm");
    return false;
  }
  delay(30);
  
  //enable BPT algo in calibration mode
  cmdStatus =writeByte(0x52, 0x04, 0x01);
  if(!cmdStatus){
    //Serial.println("failed to enable BPT algo");
    return false;
  }
  
  //Serial.println("Please keep your finger on sensor untill calibration reaches 100%");
  delay(120);

  uint8_t calibSamplBuff[30];
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
  if (cmdStatus ){
    //readCalibrationVector();
    delay(1000);
  }
  delay(1000);
  
  return true;
}


void  max32664::readCalibrationVector(){

//Disable AFE
  uint8_t ret = writeByte(0x44, 0x03, 0x00);
  delay(10);
  if(!ret){
   // Serial.println("disable afe failed");
    return false;
  }
  
  //Disable BPT algorithm
  ret = writeByte(0x52, 0x04, 0x00);
  delay(10);
  if(!ret){
   // Serial.println("failed to set raw data mode !!!");
    return false;
  }
  
/*  
  uint8_t ret = writeByte(0x51, 0x04, 0x03);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("failed to set raw data mode !!!");
    return false;
  }else{
     Serial.println("enter app mode cmd failed");
  }
*/
  //readMultipleBytes(0x51, 0x04, 0x00, &calibVector[3], 823);
  //note: it fails if we use the cmd provided in quickstart guide. should be debugged.
  
  readMultipleBytes(0x02, 0x00, &calibVector[3], 824);

  for(int i =0; i<CALIBVECTOR_SIZE; i++){
    Serial.println(calibVector[i]);
    delayMicroseconds(10);
  }
  delay(1000);  
}



void max32664::enableInterruptPin(){
//ToDo

  pinMode(MFIO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MFIO_PIN), mfioInterruptHndlr, FALLING);
  
}

uint8_t max32664::hubBegin(){

  pinMode(MFIO_PIN, OUTPUT); 
  pinMode(RESET_PIN, OUTPUT);

  digitalWrite(MFIO_PIN, HIGH); 
  digitalWrite(RESET_PIN, LOW); 
  delay(10); 
  digitalWrite(RESET_PIN, HIGH); 
  delay(1000); 
  pinMode(MFIO_PIN, INPUT_PULLUP); // To be used as an interrupt later

  //enableInterruptPin();
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
 // delay(DELAY_CMD);
  
  Wire.requestFrom(SENSORHUB_ADDR, 4);
  statusByte = Wire.read();
  
  if( statusByte ){
    
    Serial.println("read algo version failed");
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