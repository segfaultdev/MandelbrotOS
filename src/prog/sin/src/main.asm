global intsyscall

intsyscall:
  int 0x45
  ; jmp $
  ret

global _start

extern main

_start:
  call main
  jmp $
