section .data

hello: db "Hello, world!", 0x0a, 0

section .text
extern printf

global jmp

jmp:
  mov ax, (4 << 3) | 3
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  mov rax, rsp
  push (4 << 3) | 3
  push rax
  pushf
  push (3 << 3) | 3
  push testf
  iretq

testf:
  mov rdi, hello
  call printf
  ret
