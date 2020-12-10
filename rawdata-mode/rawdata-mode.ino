#include <Wire.h>
#include "max32664.h"

// Reset pin, MFIO pin
int resPin = 5;
int mfioPin = 4;
int hubAddress = 0x55;



max32664 MAX32664;

void setup(){

  Serial.begin(115200);

  Wire.begin();
  
  int result = MAX32664.hubBegin();
  if (result == CMD_SUCCESS){
    // Zero errors!
    Serial.println("Sensor started!");
  }else{
    //stay here.
    while(1){
      Serial.println("Could not communicate with the sensor!!!, hubBegin failed");
      delay(5000);
    }    
  }

  bool ret = MAX32664.configRawdataMode();
  while(!ret){      
      
      Serial.println("failed to configure Raw data mode, trying again in 10 Sec");
      ret = MAX32664.configRawdataMode();
      delay(10000);
  }
  
  Serial.println("Getting the device ready..");
  delay(2000); 
  
}

void loop(){

  static int16_t ppgBuff[300];
  static uint16_t buff_counter = 0;
  
  uint8_t no_samples = MAX32664.readRaw_Samples(&ppgBuff[buff_counter]);
  ///Serial.print("num samples ");
  //Serial.println(no_samples);

  for(int i=0; i<no_samples; i++){

    Serial.println(ppgBuff[i]);
    delay(2);
  }
  
}


uint8_t max32664::readRaw_Samples(int16_t * ppgBuff){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(ret != CMD_SUCCESS){
    Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  uint8_t readLen = 13;
  uint8_t readBuff[20]={0};

  //Read number of samples available in the fifo. function returns 0 if command fails or no new samples available
  uint8_t no_samples = readNumSamples();
  if(no_samples == 0){
    Serial.println("No samples available");
    return 0;
  }else{
    //Serial.print("num samples ");
   // Serial.println(no_samples);
  }

  for (size_t i = 0; i < no_samples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);

    unsigned long ppg0 = (unsigned long ) readBuff[0];
    ppg0 = ppg0 << 16;
    unsigned long ppg1 = (unsigned long ) readBuff[1];
    ppg1 = ppg1 << 8;
    unsigned long ppg2 = (unsigned long ) readBuff[2];
   
    unsigned long unsignedPpg = (unsigned long ) (ppg2 | ppg1 | ppg0);
    int16_t ppgFinal = (int16_t) (unsignedPpg)/10;
   
    ppgBuff[i] = ppgFinal;
  }

  return (no_samples);
}


uint8_t max32664::readNumSamples()
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(hubAddress);
  Wire.write(0x12);    
  Wire.write(0x00);    
  Wire.endTransmission();
  
  Wire.requestFrom(hubAddress, 2);
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

  Wire.beginTransmission(hubAddress);
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission();
 // delay(DELAY_CMD);
  
  Wire.requestFrom(hubAddress, readLen+1);
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



bool max32664::configRawdataMode(){

  //enter app mode
  uint8_t ret = writeByte(0x01, 0x00, 0x00);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("failed to set raw data mode !!!");
    return false;
  }else{
     Serial.println("enter app mode Success");
  }
  
  //Set output mode to sensor data 
  ret = writeByte(0x10, 0x00, 0x01);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("Set output mode to sensor data cmd failed");
    return false;
  }

//  Set sensor hub interrupt threshold
  ret = writeByte(0x10, 0x01, 0x0f);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("Set interrupt threshold cmd failed");
    return false;
  }
  
  //Enable AFE
  ret = writeByte(0x44, 0x03, 0x01);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.print("Enable AFE cmd failed   ");
    Serial.println(ret);
    return false;
  }

  //Enable BPT algorithm in Estimation mode
  ret = writeByte(0x52, 0x04, 0x02);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("failed to set raw data mode !!!");
    return false;
  }else{
     //Serial.println("Enable AFE cmd failed");
  }

  //Disable AGC.
  ret = writeByte(0x52, 0x00, 0x00);
  delay(200);
  if(ret != CMD_SUCCESS){
    Serial.println("Disable AGC cmd failed");
    return false;
  }else{
     
  }

  //led1
  ret = writeByte(0x40, 0x03, 0x0c, 0x7f);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("LED1 (red) current set failed");
    return false;
  }else{
     
  }

  //led2
  ret = writeByte(0x40, 0x03, 0x0d, 0x7f);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("LED2 (IR) current set failed");
    return false;
  }else{
     
  }


  Serial.println("Device configured in rawdata mode");
  return true;
}

void enableInterruptPin(){
//ToDo
  
}

uint8_t max32664::hubBegin(){

  pinMode(mfioPin, OUTPUT); 
  pinMode(resPin, OUTPUT);

  digitalWrite(mfioPin, HIGH); 
  digitalWrite(resPin, LOW); 
  delay(10); 
  digitalWrite(resPin, HIGH); 
  delay(1000); 
  pinMode(mfioPin, INPUT_PULLUP); // To be used as an interrupt later

  enableInterruptPin();
  uint8_t responseByte = readByte(READ_DEVICE_MODE, 0x00); // 0x00 only possible Index Byte.
  
  return responseByte; 
}




uint8_t max32664::readByte(uint8_t data1, uint8_t data2 )
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(hubAddress);
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission();
  delay(DELAY_CMD);
  
  Wire.requestFrom(hubAddress, 2); 
  statusByte = Wire.read();
  if( statusByte )
    return statusByte; // sensor returns error, check error code for more info.

  returnByte = Wire.read(); 
  return returnByte; 

}


uint8_t max32664::writeByte(uint8_t data1, uint8_t data2)
{
  Wire.beginTransmission(hubAddress);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 

  return statusByte; 
}

uint8_t max32664::writeByte(uint8_t data1, uint8_t data2, uint8_t data3)
{
  Wire.beginTransmission(hubAddress);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3); 
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  
  return statusByte; 
}

uint8_t max32664::writeByte(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4)
{
  Wire.beginTransmission(hubAddress);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3); 
  Wire.write(data4);
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 

  return statusByte; 
}
