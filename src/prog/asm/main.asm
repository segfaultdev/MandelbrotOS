section .data
msg: db 'Hello, world!', 0xD, 0xA, 0

section .text
global _start

_start:
  mov rax, 0
  mov rbx, msg
  mov rcx, 0
  mov rdx, 0

  int 0x45

  jmp $
