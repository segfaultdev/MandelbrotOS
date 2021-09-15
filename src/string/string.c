#include <stddef.h>
#include <stdint.h>
#include <string.h>

int isdigit(char c) {
  if ((c >= '0') && (c <= '9'))
    return 1;
  return 0;
}

void strcpy(char *dest, const char *source) {
  int i = 0;
  while ((dest[i] = source[i]) != '\0')
    i++;
}

void strcat(char *dest, const char *src) {
  while (*dest)
    dest++;
  while ((*dest++ = *src++))
    ;
}

size_t strlen(const char *s) {
  size_t count = 0;
  while (*s != '\0') {
    count++;
    s++;
  }
  return count;
}

int strncmp(const char *s1, const char *s2, size_t n) {
  for (size_t i = 0; i < n; i++) {
    if (s1[i] != s2[i])
      return 1;
  }

  return 0;
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
