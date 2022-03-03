#include <mm/kheap.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

int isdigit(char c) {
  if ((c >= '0') && (c <= '9'))
    return 1;
  return 0;
}

void strcpy(char *dest, char *source) {
  int i = 0;
  while ((dest[i] = source[i]) != '\0')
    i++;
}

void strcat(char *dest, char *src) {
  while (*dest)
    dest++;
  while ((*dest++ = *src++))
    ;
}

size_t strlen(char *s) {
  size_t count = 0;
  while (*s != '\0') {
    count++;
    s++;
  }
  return count;
}

int strncmp(char *s1, char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i])
      return 1;
  }

  return 0;
}

int strcmp(char *s1, char *s2) {
  while (*s1 && (*s1 == *s2))
    s1++, s2++;
  return *(char *)s1 - *(char *)s2;
}

void memmove(void *dest, void *src, size_t l) {
  uint64_t *rdest = (uint64_t *)dest;
  uint64_t *rsrc = (uint64_t *)src;
  if (src > dest)
    for (size_t i = 0; i < l; i++)
      rdest[i] = rsrc[i];
  else if (src < dest)
    for (size_t i = l; i > 0; i--)
      rdest[i - 1] = rsrc[i - 1];
}

char *strchr(char *s, int c) {
  while (*s != (char)c) {
    if (!*s++)
      return NULL;
  }
  return s;
}

char *strrchr(char *s, int c) {
  char *last = NULL;
  while (*s) {
    if (*s == (char)(c))
      last = s;
    s++;
  }
  return (char *)(last);
}

int memcmp(char *str_1, char *str_2, size_t size) {
  while (size) {
    if (*str_1 != *str_2)
      return (int)(*str_1) - (int)(*str_2);

    str_1++;
    str_2++;
    size--;
  }

  return 0;
}

char *strdup(char *s1) {
  char *str = kmalloc(strlen(s1) + 1);
  memcpy(str, s1, strlen(s1) + 1);
  return str;
}
