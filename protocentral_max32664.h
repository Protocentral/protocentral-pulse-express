#ifndef Protocentral_MAX32664_h
#define Protocentral_MAX32664_h

#include "Arduino.h"

#define MAX32664_MFIO    17
#define MAX32664_SCL     27
#define MAX32664_SDA     26
#define MAX32664_RESET   4

#define MAX32664_ADDR           0x55
#define MAX17260_ADDR           0x36
#define OUTPUT_MODE         0x10
#define WRITE_SET_THRESHOLD    0x01
#define ENABLE                 0x01
#define READ_DATA_OUTPUT       0x12
#define NUM_SAMPLES            0x0
#define ENABLE_ALGORITHM       0x52
#define ENABLE_SENSOR          0x44
#define ENABLE_MAX30101          0x03
#define ENABLE_WHRM_ALGO          0x02
#define ENABLE_WHRM_ALGO          0x02
#define READ_ALGORITHM_CONFIG          0x51
#define READ_AGC_NUM_SAMPLES_ID          0x03
#define READ_DATA                  0x01
#define READ_AGC_NUM_SAMPLES           0x00
#define MAXFAST_ARRAY_SIZE        0x06
#define SET_FORMAT             0x00
#define ENABLE_AGC_ALGO        0x00

#define ALGO_DATA             0x02
#define READ_DEVICE_MODE             0x02

#define ALGOMODE   0
#define RAWDATAMODE   1

class ProtocentralMAX32664
{
  public:
    bool begin(uint8_t mode);
    bool send_command(uint8_t * write_buffer, uint8_t len);
    void select_max32664_app_mode(void);
    bool config_algo_mode(void);
    bool config_rawdata_mode(void);
    uint8_t ReadStatus(void);
    void I2CwriteByte(uint8_t address, uint8_t subAddress, uint8_t data);
    void I2CwriteBytes(uint8_t address,  uint8_t * data, uint8_t len);
    uint8_t I2CreadByte(uint8_t address, uint8_t subAddress);
    void I2CreadBytes(uint8_t address, uint8_t subAddress, uint8_t * dest, uint8_t count);
    bool read_data_max32664(uint8_t * wrBuffer, uint16_t len, uint8_t * rdBuffer);
    bool read_sampledata_max32664(uint8_t * writeBuff, uint8_t wr_len, uint8_t * readBuff, uint8_t rd_len);
    uint8_t read_raw_samples(uint8_t * ppg_buff);
    
};

#endif
