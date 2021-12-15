#include <acpi/acpi.h>
#include <asm.h>
#include <drivers/rtc.h>
#include <printf.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

uint8_t century_register;

uint8_t rtc_get_register_raw(uint8_t reg_num) {
  outb(CMOS_ADDR, reg_num);
  return inb(CMOS_DATA);
}

int rtc_is_updating() {
  outb(CMOS_ADDR, 0x0A);
  return inb(CMOS_DATA) & 0x80;
}

uint8_t rtc_get_register(uint8_t reg_num) {
  uint8_t val = rtc_get_register_raw(reg_num);

  if (!(rtc_get_register_raw(0xb) & 4))
    return (val & 0xf) + ((val / 16) * 10);
  return val;
}

time_t rtc_get_time() {
  while (rtc_is_updating())
    ;

  return (time_t){
      .hours = rtc_get_register(4),
      .minutes = rtc_get_register(2),
      .seconds = rtc_get_register(0),
  };
}

date_t rtc_get_date() {
  while (rtc_is_updating())
    ;

  return (date_t){
      .year = (rtc_get_register(century_register) * 100) + rtc_get_register(9),
      .month = rtc_get_register(8),
      .day = rtc_get_register(7),
  };
}

datetime_t rtc_get_datetime() {
  while (rtc_is_updating())
    ;

  return (datetime_t){
      .hours = rtc_get_register(4),
      .minutes = rtc_get_register(2),
      .seconds = rtc_get_register(0),
      .year = (rtc_get_register(century_register) * 100) + rtc_get_register(9),
      .month = rtc_get_register(8),
      .day = rtc_get_register(7),
  };
}

uint8_t rtc_get_weekday() {
  while (rtc_is_updating())
    ;

  return rtc_get_register(6);
}

int init_rtc() {
  century_register = fadt->century;
  return 0;
}
