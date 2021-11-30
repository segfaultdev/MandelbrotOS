# Welcome
Welcome to the Mandelbrot Operating System, a community driven OS by the youth. 
This OS is built by a humble (Or not so humble idk) group of teenagers over at [Discord](https://discord.gg/W523cD3Q3P). 
We do this solely to have fun and to learn. 
We are not organized and are here to enjoy ourselves. 
Sounds appealing? Create a pull request!

# About 
This project is made for fun and learning.
It's like tracing OS history, but with modern knowledge and without a lot of budget lol

# Build Requirements

### Arch/Manjaro
- `sudo pacman -S base-devel qemu nasm mtools`

### Debian/Ubuntu
- `sudo apt-get install build-essential qemu nasm mtools`

# Building the Toolchain
Mandelbrot depends on some tools, like the limine bootloader and the GNU Toolchain.  
If you are on an x86\_64 system run  
```
make toolchain
```  
If you are not run
```
make toolchain_nonnative
```  
After it's done, you can head over to [Building and Running](#building-and-running).

# Building and Running
To build the OS itself, make sure you have [built the toolchain](#building-the-toolchain). After that, you can

```
make
```   
It will run the QEMU emulator by default.   

# Using
By default the OS does nothing as we don't have a userland but stuff can be added to the kernel for testing purposes. There will often be remaining test code that is left over.

# Commiting
Mandelbrot is coded to some specific standards. They are

- Use snake\_case for all variables and functions  
- Use SCREAMING\_SNAKE\_CASE for all constants  
- Use include guards in all header files and make sure they follow the following format:
```c
#ifndef __FILENAME_H__
#define __FILENAME_H__

// Code here

#endif

```   
- Try to structure the file so that you include everything first, declare all variables second and then write all functions last   
- Code in GNU99 compatible C and NASM compatible assembler
- Use stdint.h and stddef.h variable declerations for standard number sizes
- Make sure all custom typedefs end it `_t` 
- Once everything is done, clang format with the command `bash -c 'find src/. -type f \\( -iname \\*.h -o -iname \\*.c \\) -exec clang-format -i -style="{IndentWidth: 2,TabWidth: 2,IndentGotoLabels: true,IndentCaseLabels: true,KeepEmptyLinesAtTheStartOfBlocks: true}" {} \\;'`


# Extern code
Mandelbrot uses code that is not it's own. You can see a list of this code and their authors at [AUTHORS.md](/AUTHORS.md)

# Using code
This is an open source project. Reuse code. Just follow the license terms and we are good. :)

