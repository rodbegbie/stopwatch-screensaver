#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "x11shim/xshim.h"

static const char *const *g_defaults;

static const char *const *g_overrides;

void xshim_set_defaults(const char *const *defaults) { g_defaults = defaults; }
void xshim_set_overrides(const char *const *overrides) {
  g_overrides = overrides;
}

/* What xscreensaver's app-defaults file supplies to the xlockmore framework.
 * A hack's own defaults take precedence. */
static const struct {
  const char *name;
  const char *value;
} kFrameworkDefaults[] = {
    {"delta3d", "1.5"}, {"right3d", "red"},  {"left3d", "blue"},
    {"both3d", "magenta"}, {"none3d", "black"}, {"size", "0"},
};

/* Finds "<.|*>name:<ws>value" in a NULL-terminated list, or returns NULL. */
static const char *find_in(const char *const *list, const char *name) {
  size_t len = strlen(name);
  for (const char *const *p = list; p && *p; p++) {
    const char *s = *p;
    if (*s == '.' || *s == '*') s++;
    if (strncmp(s, name, len) != 0 || s[len] != ':') continue;
    s += len + 1;
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
  }
  return NULL;
}

static const char *lookup(const char *name) {
  const char *v = find_in(g_overrides, name);
  if (!v) v = find_in(g_defaults, name);
  if (v) return v;
  for (size_t i = 0; i < sizeof(kFrameworkDefaults) / sizeof(*kFrameworkDefaults);
       i++)
    if (strcmp(kFrameworkDefaults[i].name, name) == 0)
      return kFrameworkDefaults[i].value;
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

static int colour_from_spec(const char *v, uint8_t *r, uint8_t *g,
                            uint8_t *b) {
  size_t tok = 0;
  while (v[tok] && !isspace((unsigned char)v[tok])) tok++;
  for (size_t i = 0; i < sizeof(kColours) / sizeof(kColours[0]); i++)
    if (strlen(kColours[i].name) == tok &&
        strncasecmp(v, kColours[i].name, tok) == 0) {
      *r = kColours[i].r;
      *g = kColours[i].g;
      *b = kColours[i].b;
      return 1;
    }
  unsigned ur, ug, ub;
  if (*v == '#' && strlen(v) >= 7 &&
      sscanf(v + 1, "%2x%2x%2x", &ur, &ug, &ub) == 3) {
    *r = (uint8_t)ur;
    *g = (uint8_t)ug;
    *b = (uint8_t)ub;
    return 1;
  }
  return 0;
}

Status XParseColor(Display *dpy, Colormap cmap, const char *spec, XColor *c) {
  (void)dpy;
  (void)cmap;
  uint8_t r, g, b;
  if (!spec || !colour_from_spec(spec, &r, &g, &b)) return 0;
  c->red = (unsigned short)(r * 0x101);
  c->green = (unsigned short)(g * 0x101);
  c->blue = (unsigned short)(b * 0x101);
  c->flags = DoRed | DoGreen | DoBlue;
  return 1;
}

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
  uint8_t r, g, b;
  if (colour_from_spec(v, &r, &g, &b)) return rgb565(r, g, b);
  fprintf(stderr, "xshim: unknown colour '%s' for '%s'\n", v, name);
  return 0xFFFF;
}
