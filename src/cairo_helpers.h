#pragma once

#include <stddef.h>

// fwd-dcl
typedef struct _cairo cairo_t;
typedef struct _cairo cairo_t;

// Pango brings in a lot of deps, so we have a custom text layout function
// that's a lot more fun and full of bugs
// Returns number of rendered lines
size_t cairo_render_text(cairo_t *cr, const char *text, size_t ln_offset);
