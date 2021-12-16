global intsyscall

intsyscall:
  push rax
  push rbx
  push rcx
  push rdx

  push rcx
  mov rax, rdi
  mov rbx, rsi
  mov rcx, rdx
  pop rdx

  int 0x45

  pop rdx
  pop rcx
  pop rbx
  pop rax

  ret

global _start

extern cmain

_start:
  call cmain
  jmp $
