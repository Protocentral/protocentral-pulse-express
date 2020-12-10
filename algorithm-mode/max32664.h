#ifndef max32664_h
#define max32664_h

#define DELAY_CMD                 45  // Milliseconds
#define CMD_SUCCESS               0x00
#define READ_DEVICE_MODE          0x02

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
    uint8_t readSamples(uint8_t * dataBuff);
    uint8_t readCalibSamples(uint8_t * dataBuff);
    bool configAlgoInEstimationMode();
    uint8_t hubBegin();
    bool startBPTcalibration();
    bool enterAppMode();
    void enableInterruptPin();
       

  private:

    uint8_t calibVector[827] = {0};
    
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
    bool readAlgoVersion();


};
  

#endif
