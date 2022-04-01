#include <asm.h>
#include <drivers/apic.h>
#include <drivers/ps2.h>
#include <printf.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/irq.h>
#include <tasking/scheduler.h>

#define PS2_CMD_PORT 0x64
#define PS2_DATA_PORT 0x60

char curr_key = 0;
int8_t mouse_keys[3] = {0};
static uint8_t mouse_cycle = 0;

static uint8_t ps2_kbd_scancodes[128] = {
    0,   27,   '1',  '2', '3',  '4', '5', '6', '7', '8', '9', '0', '-',
    '=', '\b', '\t', 'q', 'w',  'e', 'r', 't', 'y', 'u', 'i', 'o', 'p',
    '[', ']',  '\n', 0,   'a',  's', 'd', 'f', 'g', 'h', 'j', 'k', 'l',
    ';', '\'', '`',  0,   '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',',
    '.', '/',  0,    '*', 0,    ' ', 0,   0,   0,   0,   0,   0,   0,
    0,   0,    0,    0,   0,    0,   0,   0,   0,   '-', 0,   0,   0,
    '+', 0,    0,    0,   0,    0,   0,   0,   0,   0,   0,   0,
};

void ps2_wait_out() {
  while (inb(PS2_CMD_PORT) & 2)
    ;
}

void ps2_wait_in() {
  while (!(inb(PS2_CMD_PORT) & 1))
    ;
}

void ps2_ack() {
  while (inb(PS2_DATA_PORT) != 0xfa)
    ;
}

void ps2_clr() {
  while (inb(PS2_CMD_PORT) & 1)
    inb(PS2_DATA_PORT);
}

void ps2_port1_write(uint8_t val) {
  ps2_wait_out();
  outb(PS2_DATA_PORT, val);
}

void ps2_port2_write(uint8_t val) {
  ps2_wait_out();
  outb(PS2_CMD_PORT, 0xd4);
  ps2_wait_out();
  outb(PS2_DATA_PORT, val);
}

uint8_t ps2_read() {
  ps2_wait_in();
  return inb(PS2_DATA_PORT);
}

void mouse_handler(uint64_t rsp) {
  (void)rsp;
  switch (mouse_cycle++) {
    case 0:
      mouse_keys[0] = ps2_read();
      break;
    case 1:
      mouse_keys[1] = ps2_read();
      break;
    case 2:
      mouse_keys[2] = ps2_read();
      mouse_cycle = 0;
      break;
  }
}

void kbd_handler(uint64_t rsp) {
  (void)rsp;
  ps2_wait_in();
  uint8_t c = inb(0x60);
  curr_key = ps2_kbd_scancodes[c];
}

uint8_t getchar() {
  char c = curr_key;
  curr_key = 0;
  return c;
}

void getmouse(int8_t *bytes) {
  memcpy(bytes, mouse_keys, 3);
  memset(mouse_keys, 0, 3);
}

int init_ps2() {
  asm volatile("cli");
  
  ps2_clr();

  /* Disable ports */
  ps2_wait_out();
  outb(PS2_CMD_PORT, 0xad);
  ps2_wait_out();
  outb(PS2_CMD_PORT, 0xa7);
  
  ps2_clr();

  /* ps2_wait_out(); */
  /* outb(PS2_CMD_PORT, 0x20); */
  /* ps2_wait_in(); */
  /* uint8_t conf = inb(0x60); */
  uint8_t conf = 0b01000111;
  ps2_wait_out();
  outb(PS2_CMD_PORT, 0x60);
  ps2_wait_out();
  outb(PS2_DATA_PORT, conf);

  // Enable mouse
  ps2_port2_write(0xf6);
  ps2_ack();
  ps2_port2_write(0xf3);
  ps2_ack();
  ps2_port2_write(0x14);
  ps2_ack();
  ps2_port2_write(0xf4);
  ps2_ack();

  // Enable keyboard
  ps2_wait_out();
  ps2_port1_write(0xf6);
  ps2_ack();
  
  ps2_wait_out();
  ps2_port1_write(0xf4);
  ps2_ack();

  ioapic_redirect_irq(0, 1, 33, 1);
  ioapic_redirect_irq(0, 12, 44, 1);
  irq_install_handler(1, kbd_handler);
  irq_install_handler(12, mouse_handler);

  ps2_wait_out();
  outb(PS2_CMD_PORT, 0xae);
  ps2_wait_out();
  outb(PS2_CMD_PORT, 0xa8);

  ps2_clr();
 
  asm volatile("sti");

  return 0;
}
