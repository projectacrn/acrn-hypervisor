#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "gc.h"

struct gfx_ctx {
	struct gfx_ctx_image	*gc_image;
	int raw;
};

struct gfx_ctx_image *
gc_get_image(struct gfx_ctx *gc)
{
	if (gc == NULL)
		return NULL;

	return gc->gc_image;
}
