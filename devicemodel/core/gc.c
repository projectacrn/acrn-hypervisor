#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "gc.h"

struct gfx_ctx {
	struct gfx_ctx_image	*gc_image;
	int raw;
};

struct gfx_ctx *
gc_init(int width, int height, void *fbaddr)
{
	struct gfx_ctx *gc;
	struct gfx_ctx_image *gc_image;

	gc = calloc(1, sizeof(struct gfx_ctx));
	assert(gc != NULL);

	gc_image = calloc(1, sizeof(struct gfx_ctx_image));
	assert(gc_image != NULL);

	gc_image->width = width;
	gc_image->height = height;
	if (fbaddr) {
		gc_image->data = fbaddr;
		gc->raw = 1;
	} else {
		gc_image->data = calloc(width * height, sizeof(uint32_t));
		gc->raw = 0;
	}

	gc->gc_image = gc_image;

	return gc;
}

void
gc_resize(struct gfx_ctx *gc, int width, int height)
{
	struct gfx_ctx_image *gc_image;

	gc_image = gc->gc_image;

	gc_image->width = width;
	gc_image->height = height;
	if (!gc->raw) {
		gc_image->data = realloc(gc_image->data,
			   width * height * sizeof(uint32_t));
		if (gc_image->data != NULL)
			memset(gc_image->data, 0, width * height *
			    sizeof(uint32_t));
	}
}

struct gfx_ctx_image *
gc_get_image(struct gfx_ctx *gc)
{
	if (gc == NULL)
		return NULL;

	return gc->gc_image;
}
