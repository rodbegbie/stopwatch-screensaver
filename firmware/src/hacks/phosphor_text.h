#ifndef HACKS_PHOSPHOR_TEXT_H
#define HACKS_PHOSPHOR_TEXT_H

#include <stdlib.h>

#include "hacks/phosphor_pick.h"

/* Phosphor's text source (phosphor_tty.c): a list of poems, typed a byte a
 * call, one poem a loop. A \001 byte ends a page: the source waits
 * PHOSPHOR_MORE_FRAMES calls and the next byte is the clear that starts the
 * next page. The poem's end waits PHOSPHOR_PAUSE_FRAMES and picks another,
 * never the one just shown. -1 means no byte this call. The struct has
 * textclient.h's name, `text_data`. */
#define PHOSPHOR_MORE_FRAMES 40
#define PHOSPHOR_PAUSE_FRAMES 60

struct text_data {
  const char *const *poems;
  int count;
  const char *poem;
  size_t pos;
  int wait;
  int done;
  int last;
};

static inline void phosphor_text_choose(struct text_data *td) {
  td->last = phosphor_next_poem(td->last, td->count, (unsigned long)random());
  td->poem = td->poems[td->last];
  td->pos = 0;
  td->done = 0;
}

static inline struct text_data *phosphor_text_create(const char *const *poems,
                                                     int count) {
  struct text_data *td = (struct text_data *)calloc(1, sizeof(*td));
  if (!td) return NULL;
  td->poems = poems;
  td->count = count;
  td->last = -1;
  phosphor_text_choose(td);
  return td;
}

static inline int phosphor_text_next(struct text_data *td) {
  if (td->wait > 0) {
    if (--td->wait == 0 && td->done) phosphor_text_choose(td);
    return -1;
  }
  const char c = td->poem[td->pos++];
  if (c == '\001') {
    td->wait = PHOSPHOR_MORE_FRAMES;
    return -1;
  }
  if (!c) {
    td->wait = PHOSPHOR_PAUSE_FRAMES;
    td->done = 1;
    return -1;
  }
  return (unsigned char)c;
}

#endif
