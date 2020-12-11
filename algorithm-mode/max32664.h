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

    uint8_t calibVector[827] = {0x44, 0x03, 0x00, 0, 0, 175, 63, 3, 33, 75, 0, 0, 0, 0, 15, 198, 2, 100, 3, 32, 0, 0, 3, 207, 0,      //calib vector generated with pulse-nrf
                                4, 0, 3, 175, 170, 3, 33, 134, 0, 0, 0, 0, 15, 199, 2, 100, 3, 32, 0, 0, 3,
                                207, 0, 4, 0, 3, 176, 22, 3, 33, 165, 0, 0, 0, 0, 15, 200, 2, 100, 3, 32, 0,
                                0, 3, 207, 0, 4, 0, 3, 176, 102, 3, 33, 203, 0, 0, 0, 0, 15, 201, 2, 100, 3,
                                32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 178, 3, 33, 236, 0, 0, 0, 0, 15, 202, 2,
                                100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 255, 3, 34, 16, 0, 0, 0, 0, 15,
                                203, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 64, 3, 34, 41, 0, 0, 0, 0,
                                15, 204, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 130, 3, 34, 76, 0, 0,
                                0, 0, 15, 205, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 189, 3, 34, 90,
                                0, 0, 0, 0, 15, 206, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 248, 3, 34,
                                120, 0, 0, 0, 0, 15, 207, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 178, 69, 3,
                                34, 137, 0, 0, 0, 0, 15, 208, 2, 100, 3, 32, 0, 0, 3, 0, 0, 175, 63, 3, 33,
                                75, 0, 0, 0, 0, 15, 198, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 175, 170, 3,
                                33, 134, 0, 0, 0, 0, 15, 199, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176,
                                22, 3, 33, 165, 0, 0, 0, 0, 15, 200, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3,
                                176, 102, 3, 33, 203, 0, 0, 0, 0, 15, 201, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4,
                                0, 3, 176, 178, 3, 33, 236, 0, 0, 0, 0, 15, 202, 2, 100, 3, 32, 0, 0, 3, 207,
                                0, 4, 0, 3, 176, 255, 3, 34, 16, 0, 0, 0, 0, 15, 203, 2, 100, 3, 32, 0, 0, 3,
                                207, 0, 4, 0, 3, 177, 64, 3, 34, 41, 0, 0, 0, 0, 15, 204, 2, 100, 3, 32, 0, 0,
                                3, 207, 0, 4, 0, 3, 177, 130, 3, 34, 76, 0, 0, 0, 0, 15, 205, 2, 100, 3, 32,
                                0, 0, 3, 207, 0, 4, 0, 3, 177, 189, 3, 34, 90, 0, 0, 0, 0, 15, 206, 2, 100, 3,
                                32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 248, 3, 34, 120, 0, 0, 0, 0, 15, 207, 2,
                                100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 178, 69, 3, 34, 137, 0, 0, 0, 0, 15,
                                208, 2, 100, 3, 32, 0, 0, 3, 0, 0, 175, 63, 3, 33, 75, 0, 0, 0, 0, 15, 198, 2,
                                100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 175, 170, 3, 33, 134, 0, 0, 0, 0, 15,
                                199, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 22, 3, 33, 165, 0, 0, 0, 0,
                                15, 200, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 102, 3, 33, 203, 0, 0,
                                0, 0, 15, 201, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 178, 3, 33, 236,
                                0, 0, 0, 0, 15, 202, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 176, 255, 3, 34,
                                16, 0, 0, 0, 0, 15, 203, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177, 64, 3,
                                34, 41, 0, 0, 0, 0, 15, 204, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3, 177,
                                130, 3, 34, 76, 0, 0, 0, 0, 15, 205, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4, 0, 3,
                                177, 189, 3, 34, 90, 0, 0, 0, 0, 15, 206, 2, 100, 3, 32, 0, 0, 3, 207, 0, 4,
                                0, 3, 177, 248, 3, 34, 120, 0, 0, 0, 0, 15, 207, 2, 100, 3, 32, 0, 0, 3, 207,
                                0, 4, 0, 3, 178, 69, 3, 34, 137, 0, 0, 0, 0, 15, 208, 2, 100, 3, 32, 0, 0, 3,
                                0, 0, 175, 63, 3, 33, 75, 0, 0, 0, 0, 15, 198, 2, 100, 3, 32, 0, 0, 3, 207, 0,
                                4, 0, 3, 175, 170, 3, 33, 134, 0, 0, 0, 0, 15, 199, 2, 100, 3, 32, 0, 0, 3,
                                207, 0, 4, 0, 3, 176, 22, 3, 33, 165, 0, 0, 0, 0, 15, 200, 2, 100, 3, 32, 0,
                                0, 3, 207, 0, 4, 0, 3, 176, 102, 3};
    
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
