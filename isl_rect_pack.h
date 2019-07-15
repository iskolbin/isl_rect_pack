#ifndef ISL_INCLUDE_ISL_RECT_PACK_H_
#define ISL_INCLUDE_ISL_RECT_PACK_H_

#define ISL_RECT_PACK_VERSION 1

#include <stddef.h>
#include <limits.h>

#ifndef ISLRP_NO_STDLIB
	#include <stdlib.h>
	#define ISLRP_MALLOC(v) malloc(v)
	#define ISLRP_FREE(v) free(v)
	#define ISLRP_REALLOC(a,b) realloc((a),(b))
#elif defined(ISLRP_MALLOC) || defined(ISLRP_FREE) || !defined(ISLRP_REALLOC)
	#error "You have to define ISLRP_MALLOC, ISLRP_REALLOC and ISLRP_FREE to remove stdlib.h dependency"
#endif

#ifndef ISLRP_MEMMOVE
	#ifndef ISLRP_NO_MEMMOVE
		#include <string.h>
		#define ISLRP_MEMMOVE(x,y,z) memmove((x),(y),(z))
	#else
		#define ISLRP_MEMMOVE(x,y,z) islrp__memmove((x),(y),(z))
	#endif
#endif

#ifdef ISLRP_STATIC
	#define ISLRP_DEF static
#else
	#define ISLRP_DEF extern
#endif

#ifdef __cplusplus
extern {
#endif

struct islrp_rect {
	int id;
	int page;
	int x;
	int y;
	int width;
	int height;
	int was_packed;
};

enum islrp_heuristic {
	ISLRP_METHOD_MAXRECTS_BEST_SHORT_SIDE,
	ISLRP_METHOD_MAXRECTS_BOTTOM_LEFT,
	ISLRP_METHOD_MAXRECTS_CONTACT_POINT,
	ISLRP_METHOD_MAXRECTS_BEST_LONG_SIDE,
	ISLRP_METHOD_MAXRECTS_BEST_AREA,
};

struct islrp_context {
	int pageWidth;
	int pageHeight;
	int pages;
	enum islrp_heuristic heuristic;

	struct islrp_rect *free_rects;
	size_t num_free;
	size_t allocated_free;

	struct islrp_rect *used_rects;
	size_t num_used;
	size_t allocated_used;
};

struct islrp_score_result {
	struct islrp_rect rect;
	int score1;
	int score2;
};

ISLRP_DEF void islrp_init(struct islrp_context *context, int width, int height, size_t allocate_num_rects);
ISLRP_DEF void islrp_clear(struct islrp_context *context);
ISLRP_DEF int islrp_pack_rects(struct islrp_context *context, struct islrp_rect *rects, size_t num_rects, enum islrp_heuristic heuristic);

#ifdef __cplusplus
}
#endif

#endif // ISL_INCLUDE_ISL_RECT_PACK_H_

#ifdef ISL_RECT_PACK_IMPLEMENTATION

#define ISLRP_MIN(a,b) ((a)>(b) ? (b) : (a))
#define ISLRP_ABS(a) ((a) > 0 ? (a) : -(a))

static struct islrp_score_result islrp__find_best_area(struct islrp_context *context, int width, int height) {
	size_t i;
	struct islrp_score_result result = {0};
	int best_area_fit = INT_MAX;
	int best_short_side_fit = INT_MAX;
	for ( i = 0; i < context->num_free; i++ ) {
		struct islrp_rect *rect = context->free_rects + i;
		int area_fit = rect->width * rect->height - width * height;
		if (rect->width >= width && rect->height >= height) {
			int short_side_fit = ISLRP_MIN(ISLRP_ABS(rect->width - width), ISLRP_ABS(rect->height - height));
			if (area_fit < best_area_fit || (area_fit == best_area_fit && short_side_fit < best_short_side_fit)){
				result.rect = *rect;
				best_short_side_fit = short_side_fit;
				best_area_fit = area_fit;
			}
		}
	}

	return result;
}

static struct islrp_score_result islrp__score(struct islrp_context *context, int width, int height, enum islrp_heuristic heuristic) {
	struct islrp_score_result result = {0};
	switch (heuristic) {
		//case ISLRP_METHOD_MaxRects_BestShortSide: result = islrp__find_BestShortSide( free_rects, w, h ); break;
		//case ISLRP_METHOD_MaxRects_BottomLeft: result = islrp__find_BottomLeft( free_rects, w, h ); break;
		//case ISLRP_METHOD_MaxRects_ContactPoint: result = islpr__find_ContactPoint( free_rects, w, h ); break;
		//case ISLRP_METHOD_MaxRects_BestLongSide: result = islrp__find_BestLongSide( free_rects, w, h ); break;
		case ISLRP_METHOD_MAXRECTS_BEST_AREA: result = islrp__find_best_area(context, width, height); break;
	}
	return result;
}

static int islrp__contains(struct islrp_rect *a, struct islrp_rect *b) {
	return a->x >= b->x && a->y >= b->y && a->x + a->width <= b->x + b->width && a->y + a->height <= b->y + b->height;
}

static int islrp__intersects(struct islrp_rect *a, struct islrp_rect *b) {
	return a->x <= b->x + b->width && a->x + a->width >= b->x && a->y <= b->y + b->height && a->y + a->height >= b->y;
}

static void islrp__remove_free(struct islrp_context *context, size_t i) {
	ISLRP_MEMMOVE( (void *)(context->free_rects + i), (const void *)(context->free_rects + i + 1), (context->num_free - i)*sizeof(struct islrp_rect));
	context->num_free--;
}

static void islrp__prune_free(struct islrp_context *context) {
	for (size_t i = 0; i < context->num_free; i++) {
		for (size_t j = i+1; j < context->num_free; j++) {
			if (islrp__contains(context->free_rects+i, context->free_rects+j)) {
				islrp__remove_free(context, i-- );
				break;
			}
			if (islrp__contains(context->free_rects+j, context->free_rects+i)) {
				islrp__remove_free(context, j--);
			}
		}
	}
}

static void islrp__push_free(struct islrp_context *context, struct islrp_rect rect) {
	if ( context->allocated_free >= context->num_free ) {
		context->free_rects = (struct islrp_rect *) ISLRP_REALLOC((void *)context->free_rects, 2*context->allocated_free);
		context->allocated_free *= 2;
	}
	*(context->free_rects + context->num_free++) = rect;
}

static void islrp__push_used(struct islrp_context *context, struct islrp_rect rect) {
	if ( context->allocated_used >= context->num_used ) {
		context->used_rects = (struct islrp_rect *) ISLRP_REALLOC((void *)context->used_rects, 2*context->allocated_used);
		context->allocated_used *= 2;
	}
	*(context->used_rects + context->num_used++) = rect;
}

static int islrp__split_free(struct islrp_context *context, struct islrp_rect free_rect, struct islrp_rect rect) {
	if ( !islrp__intersects(&rect, &free_rect)) {
		return 0;
	}
	
	if (rect.x < free_rect.x + free_rect.width && rect.x + rect.width > free_rect.x) {
		if (rect.y > free_rect.y && rect.y < free_rect.y + free_rect.height) {
			struct islrp_rect new_rect = free_rect;
			new_rect.height = rect.y - new_rect.y;
			islrp__push_free(context, new_rect);
		}

		if (rect.y + rect.height < free_rect.y + free_rect.height) {
			struct islrp_rect new_rect = free_rect;
			new_rect.y = rect.y + rect.height;
			new_rect.height = free_rect.y + free_rect.height - (rect.y + rect.height);
			islrp__push_free(context, new_rect);
		}
	}

	if (rect.y < free_rect.y + free_rect.height && rect.y + rect.height > free_rect.y) {
		if (rect.x > free_rect.x && rect.x < free_rect.x + free_rect.width) {
			struct islrp_rect new_rect = free_rect;
			new_rect.width = rect.x - new_rect.x;
			islrp__push_free(context, new_rect);
		}

		if (rect.x + rect.width < free_rect.x + free_rect.width) {
			struct islrp_rect new_rect = free_rect;
			new_rect.x = rect.x + rect.width;
			new_rect.width = free_rect.x + free_rect.width - (rect.x + rect.width);
			islrp__push_free(context, new_rect);
		}
	}

	return 1;
}

static void islrp__place_rect(struct islrp_context *context, struct islrp_rect rect) {
	size_t num_to_process = context->num_free;
	for (size_t i = 0; i < num_to_process; i++) {
		if (islrp__split_free(context, context->free_rects[i], rect)) {
			islrp__remove_free(context, i);
			i--;
			num_to_process--;
		}
	}
	islrp__prune_free(context);
	islrp__push_used(context, rect);
}	

int islrp_pack_rects(struct islrp_context *context, struct islrp_rect *rects, size_t num_rects, enum islrp_heuristic heuristic) {
	for (struct islrp_rect *rect = rects; i < num_rects; i++, rect++) {
		if (rect->width > context->width || rect->height > context->height) {
			return 1;
		}
	}
	for (size_t i = 0; i < num_rects; i++) {
		struct islrp_rect *rect = rects;
		struct islrp_score_result best = {{0}, INT_MAX};
		struct islrp_rect *pbest = ISLRP_NULL;
		for (size_t j = 0; j < num_rects; j++, rect++) {
			if (!rect->was_packed) {
				struct islrp_score_result result = islrp__score(context, rect->width, rect->height, heuristic);
				if (result.score1 < best.score1 || (result.score1 == best.score1 && result.score2 < best.score2)) {
					best = result;
					pbest = rect;
				}
			}
			rect++;
		}

		if (!pbest) {
			islrp__push_free(context, (struct islrp_rect){0, context->pages++, 0, 0, context->width, context->height, 0});
			i--;
		} else {
			islrp__place_rect(context, best.rect);
			pbest->was_packed = 1;
		}
	}
	return 0;
}

void islrp_init(struct islrp_context *context, int pageWidth, int pageHeight, size_t allocate_num_rects) {
	if (allocate_num_rects == 0) allocate_num_rects = 1;
	context->free_rects = ISLRP_MALLOC(sizeof(struct islrp_rect) * allocate_num_rects);
	context->free_rects[0] = (struct islrp_rect) {0, 0, 0, 0, pageWidth, pageHeight, 0};
	context->used_rects = ISLRP_MALLOC(sizeof(struct islrp_rect) * allocate_num_rects);
	context->num_free = 1;
	context->num_used = 0;
	context->allocated_free = allocate_num_rects;
	context->allocated_used = allocate_num_rects;
}

void islrp_clear(struct islrp_context *context) {
	if (context->free_rects) {
		ISLRP_FREE(context->free_rects);
		context->free_rects = ISLRP_NULL;
		context->allocated_free = 0;
		context->num_free = 0;
	}
	if (context->used_rects) {
		ISLRP_FREE(context->used_rects);
		context->used_rects = ISLRP_NULL;
		context->allocated_used = 0;
		context->num_used = 0;
	}
}

#ifdef ISLRP_NAIVE_MEMMOVE
static void *islrp__memmove(void *dest, const void *src, size_t count) {
	islrp_rect *rects_dest = (islrp_rect *) dest;
	islrp_rect *rects_src = (islrp_rect *) src;
	size_t n = count / sizeof(islrp_rect);
	while(n--) {
		*rects_dest = *rects_src;
		rects_dest++;
		rects_src++;
	}
	return dest;
}
#endif

#endif
