#include "utf8wc.h"

#include <stdlib.h>
#include <string.h>

char *utf8_to_latin1(const char *string, int ascii_p) {
  (void)ascii_p;
  for (const unsigned char *p = (const unsigned char *)string; *p; p++)
    if (*p > 127) return NULL;
  const size_t n = strlen(string) + 1;
  char *copy = (char *)malloc(n);
  if (copy) memcpy(copy, string, n);
  return copy;
}
