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



#ifndef max32664_h
#define max32664_h

#define DELAY_CMD                 45  // Milliseconds
#define CMD_SUCCESS               0x00
#define READ_DEVICE_MODE          0x02
#define RESET_PIN  4
#define MFIO_PIN 5
#define SENSORHUB_ADDR  0x55
#define CALIBVECTOR_SIZE  827
#define  RAWDATA_BUFFLEN 300


typedef struct{

  uint8_t bp_status;
  uint8_t progress;
  uint8_t sys;
  uint8_t dia;
  float   spo2;
  uint16_t ir_raw;
  uint16_t red_raw;
  uint16_t column_addr;
  uint16_t hr;

}sensorhubData;


class max32664
{
  public:
    sensorhubData max32664Output;

    uint8_t readSamples( );
    uint8_t readCalibSamples();
    bool configAlgoInEstimationMode();
    uint8_t hubBegin();
    bool startBPTcalibration();
    bool enterAppMode();

    uint8_t readRawSamples(int16_t * ppgBuff);
    bool configRawdataMode();    
       

  private:

    uint8_t readByte(uint8_t data1, uint8_t data2 );
    bool writeByte(uint8_t data1, uint8_t data2);
    bool writeByte(uint8_t data1, uint8_t data2, uint8_t data3);
    bool writeByte(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4);
    uint8_t readNumSamples();
    bool readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t * readBuff, uint8_t readLen);
    bool writeMultipleBytes(uint8_t * wrBuff, uint8_t wrLen);
    bool loadBPTcalibrationVector();
    bool setDateTime();
    bool loadSpo2Coefficients();
    bool loadSysCalibrationValues();
    bool loadDiastolicCalibrationValues();
    void readCalibrationVector();
    uint16_t formHRdata(uint8_t * hrBuff);
    float formSpo2data(uint8_t * spo2Buff);
    bool readSensorHubVersion();
};

#endif
