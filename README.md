# Welcome
Welcome to the Mandelbrot Operating System, a community driven OS by the youth. 
This OS is built by a humble (or not so humble) group of teenagers over at [Discord](https://discord.gg/W523cD3Q3P). 
We do this solely to have fun and to learn. 
We are not organized and are here to enjoy ourselves. 
Sounds appealing? Create a pull request!

# About 
This project is made for fun and learning.
It's like tracing OS history, but with modern knowledge and without a lot of (or any) budget!

# Build Requirements

### Arch-based distributions
- `sudo pacman -S base-devel qemu nasm mtools`

### Debian/Ubuntu-based distributions
- `sudo apt-get install build-essential qemu nasm mtools`

# Building the toolchain
MandelbrotOS depends on some tools, like the limine bootloader and the GNU Toolchain.

Before building the toolchain, you should fetch all submodules by doing:
```
git submodule update --init --recursive
```

Then you can proceed to build everything: 
```
make toolchain
```  

After it's done, you can head over to [Building and Running](#building-and-running).

# Building and Running
To build the OS itself, make sure you have [built the toolchain](#building-the-toolchain). After that, you can run:

```
make
```

Running make will automatically run QEMU by default.

# Usage/Current state
MandelbrotOS is still in its early stages, and thus its userspace and kernel are not developed enough to get a working shell or UI, nor to port programs. However, some basic programs with predefined paths will be loaded by the kernel.

# Contributions
MandelbrotOS tries to keep some coding standards. They are

- Use snake\_case for all variable and function names. 
- Use SCREAMING\_SNAKE\_CASE for all preprocessor macros and constants.
- Use include guards in all header files and make sure they follow the following format:

```c
#ifndef __FILENAME_H__
#define __FILENAME_H__

// (...)

#endif

```

- Try to structure the file so that you include everything first, declare all variables second and then write all functions last.  
- Code in GNU99 compatible C and NASM compatible assembly language.
- Use stdint.h and stddef.h variable declerations for standard number sizes.
- Make sure all custom typedefs end it `_t`.
- Once everything is done, make sure to format the code by running `sh clang_format.sh`.

# Extern code
MandelbrotOS uses code that is not it's own. You can see a list of used external libraries or snippets and their authors at [AUTHORS.md](/AUTHORS.md)

# Using code
This is an open source project. Reuse code. Just follow the license terms and we are good. :)

