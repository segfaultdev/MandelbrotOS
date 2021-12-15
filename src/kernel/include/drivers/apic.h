#ifndef __APIC_H__
#define __APIC_H__

#include <stdint.h>

#define SCHEDULE_REG 48

uint8_t lapic_get_id();

void lapic_eoi();
void lapic_timer_oneshot(uint8_t intr, uint32_t us);
void lapic_timer_stop();

void disable_pic();
void init_lapic();

void lapic_timer_get_freq();

void ioapic_redirect_irq(uint8_t lapic_id, uint8_t irq, uint8_t vect,
                         int status);

void lapic_send_ipi(uint8_t lapic_id, uint8_t vect);

#endif
