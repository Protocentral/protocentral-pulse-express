#include <Wire.h>
#include "max32664.h"

// Reset pin, MFIO pin
int resPin = 4;
int mfioPin = 5;
int hubAddress = 0x55;



max32664 MAX32664;
sensorhubData max32664Output;

void mfioInterruptHndlr(){
  //Serial.println("i");
}


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
      Serial.println("Could not communicate with the sensor!");
      delay(5000);
    }    
  }

  bool ret = MAX32664.startBPTcalibration();
  while(!ret){      
      
      delay(10000);
      Serial.println("failed calib, please retsart");
     // ret = MAX32664.startBPTcalibration();
  }

  delay(1000);
  //Serial.println("start in estimation mode");
  ret = MAX32664.configAlgoInEstimationMode();
  while(!ret){      
      
      //Serial.println("failed est mode");
      ret = MAX32664.configAlgoInEstimationMode();
      delay(10000);
  }
  
  //MAX32664.enableInterruptPin();
  Serial.println("Getting the device ready..");
  delay(10000); 
  
}

void loop(){

  static uint8_t readBuff[100];
  static uint16_t buff_counter = 0;

  //debug mode, Todo: implement proper read sequence.
  uint8_t num_samples = MAX32664.readSamples(readBuff);
  //Serial.print("num samples ");
  //Serial.println(num_samples);

  if(num_samples){
    
    Serial.print("sys = ");
    Serial.print(max32664Output.sys);
    Serial.print(", dia = ");
    Serial.print(max32664Output.dia);
    Serial.print(", hr = ");
    Serial.print(max32664Output.hr);  
    Serial.print(" spo2 = ");
    Serial.println(max32664Output.spo2);
    
  }else Serial.print(" NO samples availbale ");
  
  delay(100);
}


uint8_t max32664::readSamples(uint8_t  * dataBuff){

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
    Serial.println("No samples available");
    return 0;
  }else{
//    if(num_samples > MAX_SAMPL_RDSIZE) num_samples = MAX_SAMPL_RDSIZE;
    //Serial.print("num samples ");
   // Serial.println(num_samples);
  }

  for (size_t i = 0; i < num_samples; i++) {
     
    readMultipleBytes(0x12, 0x01, readBuff, readLen);

    unsigned long ppg0 = (unsigned long ) readBuff[0];
    ppg0 = ppg0 << 16;
    unsigned long ppg1 = (unsigned long ) readBuff[1];
    ppg1 = ppg1 << 8;
    unsigned long ppg2 = (unsigned long ) readBuff[2];
   
    unsigned long unsignedPpg = (unsigned long ) (ppg2 | ppg1 | ppg0);
    int16_t ppgFinal = (int16_t) (unsignedPpg)/10;
   
    //dataBuff[i] = ppgFinal;
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


uint8_t max32664::readCalibSamples(uint8_t  * dataBuff){

  uint8_t    ret = writeByte(0x00, 0x00);
  if(!ret){
    Serial.println("failed to read status, could not read samples !!!");
    return 0;
  }

  uint8_t readBuff[30]={0};
  uint8_t readLen = 23;

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
  
  //Serial.println("enter app mode");
  return true;
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

bool max32664::writeMultipleBytes(uint8_t * wrBuff, uint8_t wrLen){
  
  uint8_t returnByte;
  uint8_t statusByte;

  Wire.beginTransmission(hubAddress);

  for(int i=0; i<wrLen; i++){
    
    Wire.write(wrBuff[i]);
    delayMicroseconds(10);
  }
        
  Wire.endTransmission();
  delay(6);   //
  
  Wire.requestFrom(hubAddress, 1);
  statusByte = Wire.read();
  
  if( statusByte ){
    
    Serial.println("writeMultipleBytes cmd failed");
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
  uint8_t wrLoopCnt = (827/maxWrSize);
  uint8_t wrOffset = (827%maxWrSize);
  int i = 0;
  Serial.print("wr cnt off  ");
  Serial.println(wrLoopCnt);
  Serial.println(wrOffset);
  
  for( i=0; i<maxWrSize; i++){
     Wire.beginTransmission(hubAddress);

    for(int j=0; j<maxWrSize; j++){
      
      Wire.write(calibVector[(i*maxWrSize) + j]);
      delayMicroseconds(1);
    }
        
    Wire.endTransmission(true);
    delay(6);   //
  }

  ret = writeMultipleBytes(&calibVector[(i*maxWrSize)], wrOffset);
  delay(2);
  
  return ret;
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

  //enterAppMode();
//while(1){
  //load BPT calibration vector
  loadBPTcalibrationVector();
  delay(1000);
//}
  //set date, time
  setDateTime();
  delay(30);

  loadSpo2Coefficients();
  delay(30);

  //enable algorithm in ppg and algo mode
  writeByte(0x10, 0x00, 0x03);

  //set intr th
  writeByte(0x10, 0x01, 0x0f);

  //enable AGC
  writeByte(0x52, 0x00, 0x01);

  //enable afe
  writeByte(0x44, 0x03, 0x01);

  //enable BPT algo in estimation mode
  writeByte(0x52, 0x04, 0x02);

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
  uint8_t bpStatus = readCalibSamples(calibSamplBuff);
  
#if 1   //debug
  while(bpStatus != 2 && (max32664Output.progress != 100) ){ //0x02 == success

    bpStatus = readCalibSamples(calibSamplBuff);
    delay(1000);

    if(bpStatus == 05){    //void print_calib_vector(void)05 == failure
     // Serial.println("calibration failed");
      cmdStatus = false;
      break;
    }
  }
#endif

  //Serial.println("completed calibration");
  if (cmdStatus ){
    //readCalibrationVector();
    delay(1000);
  }
  delay(5000);
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

  for(int i =0; i<827; i++){
    Serial.println(calibVector[i]);
    delayMicroseconds(10);
  }
  delay(1000);  
}



void max32664::enableInterruptPin(){
//ToDo

  pinMode(mfioPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(mfioPin), mfioInterruptHndlr, FALLING);
  
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

  //enableInterruptPin();
  readAlgoVersion();
  
  uint8_t responseByte = readByte(READ_DEVICE_MODE, 0x00); // 0x00 only possible Index Byte.
  
  return responseByte; 
}


bool max32664::readAlgoVersion(){
  
  uint8_t returnByte;
  uint8_t statusByte;
  uint8_t readBuff[4];

  Wire.beginTransmission(hubAddress);
  Wire.write(0xff);    
  Wire.write(0x03);    
  Wire.endTransmission();
 // delay(DELAY_CMD);
  
  Wire.requestFrom(hubAddress, 4);
  statusByte = Wire.read();
  
  if( statusByte ){
    
    Serial.println("read algo version failed");
    return false;
  }else{

    Serial.print(" Version  ");
    for(int i=0; i<3; i++){
      readBuff[i] = Wire.read();
      delayMicroseconds(10);
      Serial.print(readBuff[i]);
    }

    return true;;
  }
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


bool max32664::writeByte(uint8_t data1, uint8_t data2)
{
  Wire.beginTransmission(hubAddress);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  
  if(statusByte)
    return false;
  else
    return true; 
}

bool max32664::writeByte(uint8_t data1, uint8_t data2, uint8_t data3)
{
  Wire.beginTransmission(hubAddress);     
  Wire.write(data1);    
  Wire.write(data2);    
  Wire.write(data3); 
  Wire.endTransmission(); 
  delay(DELAY_CMD); 

  Wire.requestFrom(hubAddress, 1); 
  uint8_t statusByte = Wire.read(); 
  
  if(statusByte)
    return false;
  else
    return true; 
}

bool max32664::writeByte(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4)
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

  if(statusByte)
    return false;
  else
    return true; 
}
