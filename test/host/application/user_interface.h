#pragma once
#include <stdint.h>

typedef void os_timer_func_t(void*);
typedef struct {
  os_timer_func_t* callback;
  void* argument;
  uint32_t deadline;
  bool armed;
} os_timer_t;
void os_timer_disarm(os_timer_t* timer);
void os_timer_setfn(os_timer_t* timer, os_timer_func_t* callback, void* argument);
void os_timer_arm(os_timer_t* timer, uint32_t milliseconds, bool repeat);
void system_restart();
