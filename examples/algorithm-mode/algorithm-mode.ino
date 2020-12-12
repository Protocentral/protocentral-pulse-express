//////////////////////////////////////////////////////////////////////////////////////////
//
//    Demo code for the protoCentral MAX32664 breakout board
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
      Serial.println("Could not communicate with the sensor! please make proper connections");
      delay(5000);
    }    
  }

  bool ret = MAX32664.startBPTcalibration();
  while(!ret){      
      
    delay(10000);
    Serial.println("failed calib, please retsart");
    //ret = MAX32664.startBPTcalibration();
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
  delay(1000);  
}

void loop(){

  uint8_t num_samples = MAX32664.readSamples();

  if(num_samples){
    
    Serial.print("sys = ");
    Serial.print(MAX32664.max32664Output.sys);
    Serial.print(", dia = ");
    Serial.print(MAX32664.max32664Output.dia);
    Serial.print(", hr = ");
    Serial.print(MAX32664.max32664Output.hr);  
    Serial.print(" spo2 = ");
    Serial.println(MAX32664.max32664Output.spo2);
  }
  
  delay(100);
}
