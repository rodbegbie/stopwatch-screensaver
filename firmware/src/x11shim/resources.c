#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "x11shim/xshim.h"

static const char *const *g_defaults;

void xshim_set_defaults(const char *const *defaults) { g_defaults = defaults; }

/* Finds "<.|*>name:<ws>value" and returns a pointer to the value, or NULL. */
static const char *lookup(const char *name) {
  if (!g_defaults) return NULL;
  size_t len = strlen(name);
  for (const char *const *p = g_defaults; *p; p++) {
    const char *s = *p;
    if (*s == '.' || *s == '*') s++;
    if (strncmp(s, name, len) != 0 || s[len] != ':') continue;
    s += len + 1;
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
  }
  return NULL;
}

static const struct {
  const char *name;
  uint8_t r, g, b;
} kColours[] = {
    {"black", 0, 0, 0},     {"white", 255, 255, 255}, {"red", 255, 0, 0},
    {"green", 0, 255, 0},   {"blue", 0, 0, 255},      {"yellow", 255, 255, 0},
    {"magenta", 255, 0, 255}, {"cyan", 0, 255, 255},  {"orange", 255, 165, 0},
};

static void warn_missing(const char *name) {
  fprintf(stderr, "xshim: no default for resource '%s'\n", name);
}

int get_integer_resource(Display *dpy, const char *name, const char *cls) {
  (void)dpy;
  (void)cls;
  const char *v = lookup(name);
  if (!v) {
    warn_missing(name);
    return 0;
  }
  return (int)strtol(v, NULL, 0);
}

double get_float_resource(Display *dpy, const char *name, const char *cls) {
  (void)dpy;
  (void)cls;
  const char *v = lookup(name);
  if (!v) {
    warn_missing(name);
    return 0;
  }
  return strtod(v, NULL);
}

Bool get_boolean_resource(Display *dpy, const char *name, const char *cls) {
  (void)dpy;
  (void)cls;
  const char *v = lookup(name);
  if (!v) return False;
  return strncasecmp(v, "true", 4) == 0 || strncasecmp(v, "on", 2) == 0 ||
         strncasecmp(v, "yes", 3) == 0 || *v == '1';
}

char *get_string_resource(Display *dpy, const char *name, const char *cls) {
  (void)dpy;
  (void)cls;
  const char *v = lookup(name);
  return v ? strdup(v) : NULL;
}

unsigned long get_pixel_resource(Display *dpy, Colormap cmap, const char *name,
                                 const char *cls) {
  (void)dpy;
  (void)cmap;
  (void)cls;
  const char *v = lookup(name);
  if (!v) {
    warn_missing(name);
    return 0;
  }
  size_t tok = 0;
  while (v[tok] && !isspace((unsigned char)v[tok])) tok++;
  for (size_t i = 0; i < sizeof(kColours) / sizeof(kColours[0]); i++)
    if (strlen(kColours[i].name) == tok &&
        strncasecmp(v, kColours[i].name, tok) == 0)
      return rgb565(kColours[i].r, kColours[i].g, kColours[i].b);
  if (*v == '#' && strlen(v) >= 7) {
    unsigned r, g, b;
    if (sscanf(v + 1, "%2x%2x%2x", &r, &g, &b) == 3)
      return rgb565((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }
  fprintf(stderr, "xshim: unknown colour '%s' for '%s'\n", v, name);
  return 0xFFFF;
}
