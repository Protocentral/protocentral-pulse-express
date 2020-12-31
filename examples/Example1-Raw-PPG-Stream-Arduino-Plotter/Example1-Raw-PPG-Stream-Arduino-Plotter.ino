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
//    | MFIO Pin         | 02                  |  MFIO            |
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

#define RESET_PIN 04
#define MFIO_PIN 02
#define RAWDATA_BUFFLEN 400

max32664 MAX32664(RESET_PIN, MFIO_PIN, RAWDATA_BUFFLEN);

void mfioInterruptHndlr(){

}

void enableInterruptPin(){

 // pinMode(mfioPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MAX32664.mfioPin), mfioInterruptHndlr, FALLING);
}

void setup(){

  Serial.begin(57600);

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

  static int16_t irBuff[RAWDATA_BUFFLEN];
  //static int16_t redBuff[RAWDATA_BUFFLEN];

  uint8_t num_samples = MAX32664.readRawSamples(irBuff);
  ///Serial.print("num samples ");
  //Serial.println(num_samples);

  for(int i=0; i<num_samples; i++){

    Serial.println(irBuff[i]);
    //Serial.println(redBuff[i]);
    delay(3);
  }
}
