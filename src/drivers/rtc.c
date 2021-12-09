#include <acpi/acpi.h>
#include <asm.h>
#include <drivers/rtc.h>
#include <printf.h>

#define CMOS_ADDR 0x70
#define CMOS_DATA 0x71

uint8_t century_register;

uint8_t get_rtc_register_raw(uint8_t reg_num) {
  outb(CMOS_ADDR, reg_num);
  return inb(CMOS_DATA);
}

int is_updating_rtc() {
  outb(CMOS_ADDR, 0x0A);
  return inb(CMOS_DATA) & 0x80;
}

uint8_t get_rtc_register(uint8_t reg_num) {
  uint8_t val = get_rtc_register_raw(reg_num);

  if (!(get_rtc_register_raw(0xb) & 4))
    return (val & 0xf) + ((val / 16) * 10);
  return val;
}

time_t get_time() {
  while (is_updating_rtc())
    ;

  return (time_t){
      .hours = get_rtc_register(4),
      .minutes = get_rtc_register(2),
      .seconds = get_rtc_register(0),
  };
}

date_t get_date() {
  while (is_updating_rtc())
    ;

  return (date_t){
      .year = (get_rtc_register(century_register) * 100) + get_rtc_register(9),
      .month = get_rtc_register(8),
      .day = get_rtc_register(7),
  };
}

datetime_t get_datetime() {
  while (is_updating_rtc())
    ;

  return (datetime_t){
      .hours = get_rtc_register(4),
      .minutes = get_rtc_register(2),
      .seconds = get_rtc_register(0),
      .year = (get_rtc_register(century_register) * 100) + get_rtc_register(9),
      .month = get_rtc_register(8),
      .day = get_rtc_register(7),
  };
}

uint8_t get_weekday() {
  while (is_updating_rtc())
    ;

  return get_rtc_register(6);
}

int init_rtc() {
  century_register = fadt->century;
  return 0;
}
