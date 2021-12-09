#ifndef __STRING_H__
#define __STRING_H__

#include <stddef.h>

int isdigit(char c);
void strcpy(char *dest, const char *source);
void strcat(char *dest, const char *src);
size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
void memmove(void *dest, void *src, size_t n);
char *strchr(char *s, int c);
char *strrchr(char *s, int c);
int memcmp(char *str_1, char *str_2, size_t size);

void memset(void *buffer, unsigned char value, unsigned long count);
void memset16(void *buffer, unsigned short value, unsigned long count);
void memset32(void *buffer, unsigned int value, unsigned long count);
void memset64(void *buffer, unsigned long value, unsigned long count);

void memcpy(void *dest, void *src, unsigned long length);
void memcpy16(void *dest, void *src, unsigned long length);
void memcpy32(void *dest, void *src, unsigned long length);
void memcpy64(void *dest, void *src, unsigned long length);

#endif
