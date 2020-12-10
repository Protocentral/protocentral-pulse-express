#include <SparkFun_Bio_Sensor_Hub_Library.h>
#include <Wire.h>

// Reset pin, MFIO pin
int resPin = 4;
int mfioPin = 5;
int hubAddress = 0x55;


#define DELAY_CMD                 45  // Milliseconds
#define CMD_SUCCESS               0x00


void setup(){

  Serial.begin(115200);

  Wire.begin();
  int result = hub_begin();
  
  if (result == 0){
    // Zero errors!
    Serial.println("Sensor started!");
  }else{
    //stay here.
    while(1){
      Serial.println("Could not communicate with the sensor!!!, hub_begin failed");
      delay(5000);
    }    
  }

  bool ret = configRawdataMode();
  while(!ret){
      
      Serial.println("failed to configure Raw data mode, trying again in 3 Sec");
      ret = configRawdataMode();
      delay(3000);
  }
  
  // Data lags a bit behind the sensor, if you're finger is on the sensor when
  // it's being configured this delay will give some time for the data to catch
  // up. 
  Serial.println("Loading up the buffer with data....");
  delay(4000); 
  
}

void loop(){

  static int16_t ppgBuff[300];
  static uint16_t buff_counter = 0;
  
  uint8_t no_samples = read_raw_samples(&ppgBuff[buff_counter]);
  ///Serial.print("num samples ");
  //Serial.println(no_samples);

    
  for(int i=0; i<no_samples; i++){

    Serial.println(ppgBuff[i]);
    delay(2);
  }
  
  //delay(10);    //ToDo: read saples based on interrupt 
}


uint8_t read_raw_samples(int16_t * ppgBuff){

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

    unsigned long ppg0 = (unsigned long ) readBuff[0];//readBuff[1];
    ppg0 = ppg0 << 16;
    unsigned long ppg1 = (unsigned long ) readBuff[1];
    ppg1 = ppg1 << 8;
   
    unsigned long ppg2 = (unsigned long ) readBuff[2];
   // ppg2 = ppg2 << 8;
   
    unsigned long unsignedPpg = (unsigned long ) (ppg2 | ppg1 | ppg0);

    //signed long signedPpg = (signed long) unsignedPpg;
    //signedPpg =  signedPpg >> 8;
    //int16_t ppgFinal = (int16_t) (signedPpg)/10;
    int16_t ppgFinal = (int16_t) (unsignedPpg)/10;
   
    static uint8_t count = 0;
    ppgBuff[i] = ppgFinal;
    //ppgBuff[i] = count++;

  //  delay(1);//debug
  }

    return (no_samples);
}

uint8_t readNumSamples()
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(hubAddress);
  Wire.write(0x12);    
  Wire.write(0x00);    
  Wire.endTransmission();
  //delay(DELAY_CMD);
  //delay(6);
  
  Wire.requestFrom(hubAddress, 2);
  statusByte = Wire.read();
  if( statusByte ){
    Serial.println("read num of samples cmd failed");
    return 0;
  }else{
    returnByte = Wire.read(); 
    return returnByte;
  }
}

bool readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t * readBuff, uint8_t readLen){
  
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



bool configRawdataMode(){
/*
    // Check that communication was successful, not that the sensor is enabled.
  uint8_t statusByte = enableWrite(ENABLE_SENSOR, ENABLE_MAX30101, ENABLE);
  if( statusByte != SUCCESS ){
     Serial.println("enable write failed");
     return false; 
  }else{
    return true;; 
  }
  */
  //enter app mode
  uint8_t ret = writeByte(0x01, 0x00, 0x00);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("enter app mode cmd failed");
    return false;
  }
  //Set output mode to sensor data 
  ret = writeByte(0x10, 0x00, 0x01);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("Set output mode to sensor data cmd failed");
    return false;
  }

  //Set sensor hub interrupt threshold
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
    Serial.println("Enable AFE cmd failed");
    return false;
  }

  //Enable BPT algorithm in Estimation mode
  ret = writeByte(0x52, 0x04, 0x02);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("Enable AFE cmd failed");
    return false;
  }

  //Disable AGC.
  ret = writeByte(0x52, 0x00, 0x00);
  delay(200);
  if(ret != CMD_SUCCESS){
    Serial.println("Disable AGC cmd failed");
    return false;
  }

  //led1
  ret = writeByte(0x40, 0x03, 0x0c, 0x7f);
  delay(10);
  if(ret != CMD_SUCCESS){
     Serial.println("LED1 (red) current set failed");
    return false;
  }
  
  //led2
  ret = writeByte(0x40, 0x03, 0x0d, 0x7f);
  delay(10);
  if(ret != CMD_SUCCESS){
    Serial.println("LED2 (IR) current set failed");
    return false;
  }


  Serial.println("Device configured in rawdata mode");
  return true;
}


uint8_t hub_begin(){

  pinMode(mfioPin, OUTPUT); 
  pinMode(resPin, OUTPUT);

  digitalWrite(mfioPin, HIGH); 
  digitalWrite(resPin, LOW); 
  delay(10); 
  digitalWrite(resPin, HIGH); 
  delay(1000); 
  pinMode(mfioPin, INPUT_PULLUP); // To be used as an interrupt later

  readAlgoVersion();
  
  uint8_t responseByte = readByte(READ_DEVICE_MODE, 0x00); // 0x00 only possible Index Byte.
  return responseByte; 
}




uint8_t readByte(uint8_t _familyByte, uint8_t _indexByte )
{

  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(hubAddress);
  Wire.write(_familyByte);    
  Wire.write(_indexByte);    
  Wire.endTransmission();
  delay(DELAY_CMD);
  
  Wire.requestFrom(hubAddress, 2); 
  statusByte = Wire.read();
  if( statusByte )// SUCCESS (0x00) - how do I know its 
    return statusByte; // Return the error, see: READ_STATUS_BYTE_VALUE 

  returnByte = Wire.read(); 
  return returnByte; // If good then return the actual byte. 

}

// This function uses the given family, index, and write byte to enable
// the given sensor. 
uint8_t enableWrite(uint8_t _familyByte, uint8_t _indexByte, uint8_t _enableByte)
{

  Wire.beginTransmission(hubAddress);     
  Wire.write(_familyByte);    
  Wire.write(_indexByte);    
  Wire.write(_enableByte); 
  Wire.endTransmission(); 
  delay(ENABLE_CMD_DELAY); 

  // Status Byte, success or no? 0x00 is a successful transmit
  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  return statusByte; 

}

uint8_t writeByte(uint8_t _familyByte, uint8_t _indexByte)
{

  Wire.beginTransmission(hubAddress);     
  Wire.write(_familyByte);    
  Wire.write(_indexByte);    
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  // Status Byte, success or no? 0x00 is a successful transmit
  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  return statusByte; 

}

uint8_t writeByte(uint8_t _familyByte, uint8_t _indexByte, uint8_t _writeByte)
{

  Wire.beginTransmission(hubAddress);     
  Wire.write(_familyByte);    
  Wire.write(_indexByte);    
  Wire.write(_writeByte); 
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  // Status Byte, success or no? 0x00 is a successful transmit
  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  return statusByte; 

}

bool readAlgoVersion(){
  
  uint8_t returnByte;
  uint8_t statusByte;
  uint8_t readBuff[4];

  Wire.beginTransmission(hubAddress);
  Wire.write(0xff);    
  Wire.write(0x03);    
  Wire.endTransmission();
  delay(DELAY_CMD);
  
  Wire.requestFrom(hubAddress, 4);
  statusByte = Wire.read();
  
  if( statusByte ){
    
    Serial.println("read algo version failed");
    return false;
  }else{

    Serial.print("Algo Version  ");
    for(int i=0; i<3; i++){
      readBuff[i] = Wire.read();
      delayMicroseconds(10);
      Serial.print(readBuff[i]);
    }
    Serial.println(".");
    
    return true;;
  }
}

uint8_t writeByte(uint8_t _familyByte, uint8_t _indexByte, uint8_t _writeByte1, uint8_t _writeByte2)
{

  Wire.beginTransmission(hubAddress);     
  Wire.write(_familyByte);    
  Wire.write(_indexByte);    
  Wire.write(_writeByte1); 
  Wire.write(_writeByte2);
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  // Status Byte, success or no? 0x00 is a successful transmit
  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  return statusByte; 

}
