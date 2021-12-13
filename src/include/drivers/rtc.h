#ifndef __RTC_H__
#define __RTC_H__

#include <stdint.h>

typedef struct time {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} time_t;

typedef struct date {
  uint8_t weekday;

  uint8_t day;
  uint8_t month;
  uint16_t year;
} date_t;

typedef struct datetime {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;

  uint8_t day;
  uint8_t month;
  uint16_t year;
} datetime_t;

time_t rtc_get_time();
date_t rtc_get_date();
datetime_t rtc_get_datetime();
int init_rtc();

#endif
