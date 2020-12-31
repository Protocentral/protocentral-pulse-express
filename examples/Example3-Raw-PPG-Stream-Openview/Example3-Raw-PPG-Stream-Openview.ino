//////////////////////////////////////////////////////////////////////////////////////////
//
//    Demo code for the protoCentral MAX32664 breakout board
//
//    Author: Joice Tm
//    Copyright (c) 2020 ProtoCentral
//
//    This example plots the raw PPG signal through serial UART on openview processing GUI.
//    GUI URL: https://github.com/Protocentral/protocentral_openview.git
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

#define CES_CMDIF_PKT_START_1   0x0A
#define CES_CMDIF_PKT_START_2   0xFA
#define CES_CMDIF_TYPE_DATA     0x02
#define CES_CMDIF_PKT_STOP      0x0B
#define DATA_LEN                9
#define ZERO                    0

#define RESET_PIN 04
#define MFIO_PIN 02
#define RAWDATA_BUFFLEN 200

volatile char DataPacket[DATA_LEN];
const char DataPacketFooter[2] = {ZERO, CES_CMDIF_PKT_STOP};
const char DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2, DATA_LEN, ZERO, CES_CMDIF_TYPE_DATA};

max32664 MAX32664(RESET_PIN, MFIO_PIN, RAWDATA_BUFFLEN);

void mfioInterruptHndlr(){

}

void enableInterruptPin(){

 // pinMode(mfioPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MAX32664.mfioPin), mfioInterruptHndlr, FALLING);
}

void sendDataThroughUart(int16_t ir, int16_t red){

  DataPacket[0] = ir;
  DataPacket[1] = ir >> 8;
  DataPacket[2] = red;
  DataPacket[3] = red >> 8;

  //send packet header
  for(int i=0; i<5; i++){

    Serial.write(DataPacketHeader[i]);
  }

  //send actual data
  for(int i=0; i<DATA_LEN; i++){

    Serial.write(DataPacket[i]);
  }

  //send packet footer
  for(int i=0; i<2; i++){

    Serial.write(DataPacketFooter[i]);
  }
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
  static int16_t redBuff[RAWDATA_BUFFLEN];

  uint8_t num_samples = MAX32664.readRawSamples(irBuff, redBuff);
  ///Serial.print("num samples ");
  //Serial.println(num_samples);

  for(int i=0; i<num_samples; i++){

    sendDataThroughUart(irBuff[i], redBuff[i]);
    delay(2);
  }
}
