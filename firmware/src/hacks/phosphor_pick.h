#ifndef HACKS_PHOSPHOR_PICK_H
#define HACKS_PHOSPHOR_PICK_H

/* The next poem for Phosphor to type: any of `count` but `last` (-1 for none
 * yet), chosen by `r`, a random number. Taking r modulo count - 1 and stepping
 * past `last` makes every other poem equally likely. */
static inline int phosphor_next_poem(int last, int count, unsigned long r) {
  if (count < 2) return 0;
  if (last < 0 || last >= count) return (int)(r % (unsigned long)count);
  const int n = (int)(r % (unsigned long)(count - 1));
  return n >= last ? n + 1 : n;
}

#endif
