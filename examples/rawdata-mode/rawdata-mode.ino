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
//    Place your finger on the sebsor and open arduino serial plotter to view the ppg signal.
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


#include "max32664.h"
#include <Wire.h>

max32664 MAX32664(04/*Reset Pin*/, 05/* MFIO pin*/);

void mfioInterruptHndlr(){
  
}

void enableInterruptPin(){
  
 // pinMode(mfioPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(mfioPin), mfioInterruptHndlr, FALLING);
  
}

void setup(){

  Serial.begin(115200);

  Wire.begin();
  int result = MAX32664.hubBegin();
  
  if (result == 0){
    Serial.println("Sensor started!");
  }else{
    
    //stay here.
    while(1){
      Serial.println("Could not communicate with the sensor!!!");
      delay(30000);
    }    
  }

  bool ret = MAX32664.configRawdataMode();
  while(!ret){
      
    Serial.println("failed to configure Raw data mode, trying again in 30 Sec");
    ret = MAX32664.configRawdataMode();
    delay(30000);
  }

  Serial.println("Geting the device ready..");
  delay(2000); 
  
}

void loop(){

  static int16_t ppgBuff[RAWDATA_BUFFLEN];
  static uint16_t buff_counter = 0;
  
  uint8_t no_samples = MAX32664.readRawSamples(&ppgBuff[buff_counter]);
  ///Serial.print("num samples ");
  //Serial.println(no_samples);

  for(int i=0; i<no_samples; i++){

    Serial.println(ppgBuff[i]);
    delay(2);
  }
}

