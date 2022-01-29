#include <acpi/acpi.h>
#include <asm.h>
#include <drivers/rtc.h>
#include <printf.h>
#include <stdint.h>

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

datetime_t rtc_get_datetime() {
  while (rtc_is_updating())
    ;

  return (datetime_t){
      .hours = rtc_get_register(4),
      .minutes = rtc_get_register(2),
      .seconds = rtc_get_register(0),
      .year = (rtc_get_register(century_register) * 100) + rtc_get_register(9) -
              1900, // POSIX
      .month = rtc_get_register(8),
      .day = rtc_get_register(7),

      .day_of_week = rtc_get_register(6) - 1,
      .day_of_year = 0, // TODO: Actually impliment this
      .is_daylight_savings =
          0, // TODO: Make the poor humans who have DST actually be supported
  };
}

static inline uint64_t get_julian_day(uint8_t days, uint8_t months,
                                      uint16_t years) {
  return (1461 * (years + 4800 + (months - 14) / 12)) / 4 +
         (367 * (months - 2 - 12 * ((months - 14) / 12))) / 12 -
         (3 * ((years + 4900 + (months - 14) / 12) / 100)) / 4 + days - 32075;
}

posix_time_t rtc_mktime(datetime_t dt) {
  uint64_t jdn_current = get_julian_day(dt.day, dt.month, dt.year);
  uint64_t jdn_1970 = get_julian_day(1, 1, 1970);

  uint64_t jdn_diff = jdn_current - jdn_1970;

  return (posix_time_t){.seconds = (jdn_diff * (60 * 60 * 24)) +
                                   dt.hours * 3600 + dt.minutes * 60 +
                                   dt.seconds,
                        .nanoseconds = 0};
}

int init_rtc() {
  century_register = fadt->century;
  return 0;
}
