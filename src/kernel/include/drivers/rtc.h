#ifndef __RTC_H__
#define __RTC_H__

#include <stddef.h>
#include <stdint.h>

typedef struct datetime {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;

  uint8_t day;
  uint8_t month;
  uint16_t year;

  uint8_t day_of_week;
  uint8_t day_of_year;
  int is_daylight_savings;
} datetime_t; // Equivilant to struct tm

typedef struct posix_time {
  size_t seconds;
  size_t nanoseconds;
} posix_time_t; // Equivilant to struct timespec

datetime_t rtc_get_datetime();
posix_time_t rtc_mktime(datetime_t dt);
int init_rtc();

#endif
