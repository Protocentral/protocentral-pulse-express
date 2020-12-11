#ifndef max32664_h
#define max32664_h

#define DELAY_CMD                 45  // Milliseconds
#define CMD_SUCCESS               0x00
#define READ_DEVICE_MODE          0x02

class max32664
{
  public:
    uint8_t readRaw_Samples(int16_t * ppgBuff);
    bool configRawdataMode();
    uint8_t hubBegin();

       

  private:
    uint8_t readByte(uint8_t data1, uint8_t data2 );
    uint8_t writeByte(uint8_t data1, uint8_t data2);
    uint8_t writeByte(uint8_t data1, uint8_t data2, uint8_t data3);
    uint8_t writeByte(uint8_t data1, uint8_t data2, uint8_t data3, uint8_t data4);
    uint8_t readNumSamples();
    bool readMultipleBytes(uint8_t data1, uint8_t data2, uint8_t * readBuff, uint8_t readLen);
};
  

#endif
