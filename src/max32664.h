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

#define SENSORHUB_ADDR  0x55
#define CALIBVECTOR_SIZE  827
#define  RAWDATA_BUFFLEN 200
#define  ERR_UNKNOWN      0xff


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

typedef struct{

  uint8_t ResetPin;
  uint8_t mfioPin;
  uint8_t calibValSys[3];
  uint8_t calibValDia[3];

  float spo2CalibCoefA; 
  float spo2CalibCoefB; 
  float spo2CalibCoefC;

}algomodeInitialiser;


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

    uint8_t readRawSamples(int16_t * irBuff, int16_t * redBuff);
    uint8_t readRawSamples(int16_t * irBuff);
    bool configRawdataMode();
    void loadAlgorithmParameters(algomodeInitialiser * algoParameters);

    uint8_t ResetPin;
    uint8_t mfioPin;
    uint8_t calibValSys[3];
    uint8_t calibValDia[3];

    float spo2CalibCoefA; 
    float spo2CalibCoefB; 
    float spo2CalibCoefC;
    uint8_t rawDataBuffLen;
    
    max32664(uint8_t reset, uint8_t mfio, uint16_t buffLen){
      ResetPin = reset;
      mfioPin = mfio;
      rawDataBuffLen = buffLen;
    }


  private:
 
    uint8_t readByte(uint8_t data1, uint8_t data2 );
    uint8_t readByte(uint8_t data1, uint8_t data2, uint8_t data3 );
    bool writeByte(uint8_t data1, uint8_t data2);
    bool writeByte(uint8_t data1, uint8_t data2, uint8_t data3);
    bool writeByte(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4);
    uint8_t readNumSamples();
    bool readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t * readBuff, uint8_t readLen);
    bool readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t * readBuff, uint8_t readLen);
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
    bool readSensorHubAlgoVersion(void);

    bool setFifoIntrThreshold(uint8_t threshold);
    bool writeMax301xxReg(uint8_t regAddr, uint8_t val);
    uint8_t readMax301xxReg(uint8_t regAddr);
    uint8_t readAGCalgoNumSamplestoAvg(void);
    bool SetAGCalgoNumSamplestoAvg(uint8_t numSamples);
    uint8_t readAGCalgoSensitivity(void);
    uint8_t readAGCalgoStepSize(void);
    uint8_t readTargetPercentagefullFcaleADC(void);
    bool SetAGCalgoSensitivity(uint8_t percent);
    bool SetAGCalgoStepSize(uint8_t percent);
    bool SetTargetPercentagefullFcaleADC(uint8_t percent);
    
};

#endif
