#include <Wire.h>
#include "max32664.h"

max32664 MAX32664;


void mfioInterruptHndlr(){
  //Serial.println("i");
}

void enableInterruptPin(){
//ToDo

  pinMode(MFIO_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MFIO_PIN), mfioInterruptHndlr, FALLING);
  
}

void setup(){

  Serial.begin(115200);

  Wire.begin();
  
  int result = MAX32664.hubBegin();
  if (result == CMD_SUCCESS){
    Serial.println("Sensorhub begin!");
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
  delay(3000); 
  
}

void loop(){

  //Todo: Read samples based on interrupt.
  uint8_t num_samples = MAX32664.readSamples();
  //Serial.print("num samples ");
  //Serial.println(num_samples);

  if(num_samples){
    
    Serial.print("sys = ");
    Serial.print(MAX32664.max32664Output.sys);
    Serial.print(", dia = ");
    Serial.print(MAX32664.max32664Output.dia);
    Serial.print(", hr = ");
    Serial.print(MAX32664.max32664Output.hr);  
    Serial.print(" spo2 = ");
    Serial.println(MAX32664.max32664Output.spo2);
    
  }//else Serial.print(" NO samples availbale ");
  
  delay(100);
}
