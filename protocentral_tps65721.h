#ifndef protocentral_tps65721_h
#define protocentral_tps65721_h

#define TPS65721_ADDR 0x48
#define CHGCONFIG0 0x02
#define CHGCONFIG1 0x03
#define CHGCONFIG2 0x04
#define CHGCONFIG3 0x05
#define CHGSTATUS 0x01


bool InitTps65721(void);
void tps65721_intr_handler(void);
bool check_charging_status(void);
void go_tocharge_mode(void);
void tps65721_write(uint8_t address, uint8_t subAddress, uint8_t data);

uint8_t tps65721_read(uint8_t address, uint8_t subAddress);
void turn_off_leds_tps(void);
void set_green_led_tps(void);
void set_blue_led_tps(void);

#endif