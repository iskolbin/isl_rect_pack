#ifndef ISL_INCLUDE_ISL_RECT_PACK_H_
#define ISL_INCLUDE_ISL_RECT_PACK_H_

#define ISL_RECT_PACK_VERSION 1

#ifndef ISLRP_NO_STDDEF
	#include <stddef.h>
	typedef size_t islrp_size_t;
#else
	typedef unsigned long int islrp_size_t;
#endif

#ifndef ISLRP_NO_LIMITS
	#include <limits.h>
	#define ISLRP_INT_MAX INT_MAX
#else
	#define ISLRP_INT_MAX (2*((1<<((sizeof(int)<<3)-2))-1)+1)
#endif

#if !defined(ISLRP_NO_AUTO_RESIZE) && defined(ISLRP_NO_STDLIB) && !defined(ISLRP_REALLOC)
	#error "You must disable auto resizing, either allow use of stdlib or define realloc"
#endif

#if defined(ISLRP_MALLOC) && defined(ISLRP_FREE)
	#define ISLRP_NO_STDLIB
#endif

#ifndef ISLRP_NO_STDLIB
	#include <stdlib.h>
	#define ISLRP_NULL NULL
	#ifndef ISLRP_MALLOC
		#define ISLRP_MALLOC(v) malloc(v)
	#endif
	#ifndef ISLRP_FREE
		#define ISLRP_FREE(v) free(v)
	#endif
	#ifndef ISLRP_REALLOC
		#define ISLRP_REALLOC(a,b) realloc((a),(b))
	#endif
#else
	#define ISLRP_NULL ((void *)0)
#endif

#ifndef ISLRP_MEMMOVE
	#ifndef ISLRP_NO_MEMMOVE
		#include <string.h>
		#define ISLRP_MEMMOVE(x,y,z) memmove((x),(y),(z))
	#else
		#define ISLRP_NAIVE_MEMMOVE
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

typedef struct {
	int id;
	int x;
	int y;
	int w;
	int h;
	int was_packed;
} islrp_rect;

typedef struct {
	islrp_rect *free_rects;
	islrp_rect *used_rects;
	islrp_size_t num_free;
	islrp_size_t num_used;
	islrp_size_t allocated_free;
	islrp_size_t allocated_used;
} islrp_context;

typedef struct {
	islrp_rect rect;
	int score1;
	int score2;
} islrp__score_result;

typedef enum {
	ISLRP_METHOD_MAXRECTS_BEST_SHORT_SIDE,
	ISLRP_METHOD_MAXRECTS_BOTTOM_LEFT,
	ISLRP_METHOD_MAXRECTS_CONTACT_POINT,
	ISLRP_METHOD_MAXRECTS_BEST_LONG_SIDE,
	ISLRP_METHOD_MAXRECTS_BEST_AREA,
} islrp_method;

ISLRP_DEF int islrp_pack_rects( islrp_context *context, islrp_rect *rects, islrp_size_t num_rects, islrp_method method );
#ifndef ISLRP_NO_STDLIB
ISLRP_DEF islrp_context *islrp_create_default_context( int prealloc );
ISLRP_DEF void islrp_destroy_default_context( islrp_context *context );
#endif

#ifdef __cplusplus
}
#endif

#endif // ISL_INCLUDE_ISL_RECT_PACK_H_

#ifdef ISL_RECT_PACK_IMPLEMENTATION

#define ISLRP_MIN(a,b) ((a)>(b) ? (b) : (a))
#define ISLRP_ABS(a) ((a) > 0 ? (a) : -(a))

static islrp__score_result islrp__find_best_area( islrp_context *context, int w, int h ) {
	islrp_size_t i;
	islrp__score_result result = {0};
	
	int best_area_fit = ISLRP_INT_MAX;
	int best_short_side_fit = ISLRP_INT_MAX;
	
	for ( i = 0; i < context->num_free; i++ ) {
		islrp_rect *rect = context->free_rects + i;
		int area_fit = rect->w * rect->h - w * h;
		if ( rect->w >= w && rect->h >= h ) {
			int short_side_fit = ISLRP_MIN( ISLRP_ABS( rect->w - w ), ISLRP_ABS( rect->h - h ));
			if ( area_fit < best_area_fit || ( area_fit == best_area_fit && short_side_fit < best_short_side_fit )){
				result.rect = *rect;
				best_short_side_fit = short_side_fit;
				best_area_fit = area_fit;
			}
		}
	}

	return result;
}

static islrp__score_result islrp__score( islrp_context *context, int w, int h, islrp_method method ) {
	//islrp_rect rect = {0};
	islrp__score_result result = {0};//rect,INT_MAX,INT_MAX};
	switch ( method ) {
		//case ISLRP_METHOD_MaxRects_BestShortSide: result = islrp__find_BestShortSide( free_rects, w, h ); break;
		//case ISLRP_METHOD_MaxRects_BottomLeft: result = islrp__find_BottomLeft( free_rects, w, h ); break;
		//case ISLRP_METHOD_MaxRects_ContactPoint: result = islpr__find_ContactPoint( free_rects, w, h ); break;
		//case ISLRP_METHOD_MaxRects_BestLongSide: result = islrp__find_BestLongSide( free_rects, w, h ); break;
		case ISLRP_METHOD_MAXRECTS_BEST_AREA: result = islrp__find_best_area( context, w, h ); break;
	}
	return result;
}

static int islrp__contains( islrp_rect *a, islrp_rect *b ) {
	return a->x >= b->x && a->y >= b->y && a->x + a->w <= b->x + b->w && a->y + a->h <= b->y + b->h;
}

static int islrp__intersects( islrp_rect *a, islrp_rect *b ) {
	return a->x <= b->x + b->w && a->x + a->w >= b->x && a->y <= b->y + b->h && a->y + a->h >= b->y;
}

static void islrp__remove_free( islrp_context *context, islrp_size_t i ) {
	ISLRP_MEMMOVE( (void *)(context->free_rects + i), (const void *)(context->free_rects + i + 1), (context->num_free - i)*sizeof(islrp_rect));
	context->num_free--;
}

static void islrp__prune_free( islrp_context *context ) {
	islrp_size_t i, j;
	for ( i = 0; i < context->num_free; i++ ) {
		for ( j = i+1; j < context->num_free; j++ ) {
			if ( islrp__contains( context->free_rects+i, context->free_rects+j )) {
				islrp__remove_free( context, i-- );
				break;
			}
			if ( islrp__contains( context->free_rects+j, context->free_rects+i )) {
				islrp__remove_free( context, j-- );
			}
		}
	}
}

static void islrp__push_free( islrp_context *context, islrp_rect rect ) {
	if ( context->allocated_free >= context->num_free ) {
#ifndef ISLRP_NO_AUTO_RESIZE
		context->free_rects = (islrp_rect *) ISLRP_REALLOC( (void *)context->free_rects, 2*context->allocated_free );
		context->allocated_free *= 2;
#endif
	}

	*(context->free_rects + context->num_free++) = rect;
}

static void islrp__push_used( islrp_context *context, islrp_rect rect ) {
	if ( context->allocated_used >= context->num_used ) {
#ifndef ISLRP_NO_AUTO_RESIZE
		context->used_rects = (islrp_rect *) ISLRP_REALLOC( (void *)context->used_rects, 2*context->allocated_used );
		context->allocated_used *= 2;
#endif
	}

	*(context->used_rects + context->num_used++) = rect;
}

static int islrp__split_free( islrp_context *context, islrp_rect free_rect, islrp_rect rect ) {
	if ( !islrp__intersects( &rect, &free_rect )) {
		return 0;
	}
	
	if (rect.x < free_rect.x + free_rect.w && rect.x + rect.w > free_rect.x) {
		if (rect.y > free_rect.y && rect.y < free_rect.y + free_rect.h) {
			islrp_rect new_rect = free_rect;
			new_rect.h = rect.y - new_rect.y;
			islrp__push_free( context, new_rect );
		}

		if (rect.y + rect.h < free_rect.y + free_rect.h) {
			islrp_rect new_rect = free_rect;
			new_rect.y = rect.y + rect.h;
			new_rect.h = free_rect.y + free_rect.h - (rect.y + rect.h);
			islrp__push_free( context, new_rect );
		}
	}

	if (rect.y < free_rect.y + free_rect.h && rect.y + rect.h > free_rect.y) {
		if (rect.x > free_rect.x && rect.x < free_rect.x + free_rect.w) {
			islrp_rect new_rect = free_rect;
			new_rect.w = rect.x - new_rect.x;
			islrp__push_free( context, new_rect );
		}

		if (rect.x + rect.w < free_rect.x + free_rect.w) {
			islrp_rect new_rect = free_rect;
			new_rect.x = rect.x + rect.w;
			new_rect.w = free_rect.x + free_rect.w - (rect.x + rect.w);
			islrp__push_free( context, new_rect );
		}
	}

	return 1;
}

static void islrp__place_rect( islrp_context *context, islrp_rect rect ) {
	islrp_size_t i;
	islrp_size_t num_free = context->num_free;
	for ( i = 0; i < num_free; i++ ) {
		if ( islrp__split_free( context, context->free_rects[i], rect )) {
			islrp__remove_free( context, i );
			i--;
			num_free--;
		}
	}

	islrp__prune_free( context );
	islrp__push_used( context, rect );
}	

int islrp_pack_rects( islrp_context *context, islrp_rect *rects, islrp_size_t num_rects, islrp_method method ) {
	islrp_size_t i, j;
	for ( i = 0; i < num_rects; i++ ) {
		islrp_rect *iter = rects;
		islrp__score_result best = {{0},ISLRP_INT_MAX,ISLRP_INT_MAX};
		islrp_rect *pbest = ISLRP_NULL;
		for ( j = 0; j < num_rects; j++, iter++ ) {
			if ( !iter->was_packed ) {
				islrp__score_result result = islrp__score( context, iter->w, iter->h, method );
				if ( result.score1 < best.score1 || (result.score1 == best.score1 && result.score2 < best.score2)) {
					best = result;
					pbest = iter;
				}
			}
			iter++;
		}

		if ( !pbest ) {
			return 1;
		} else {
			islrp__place_rect( context, best.rect );
			pbest->was_packed = 1;
		}
	}
	return 0;
}

#ifndef ISLRP_NO_STDLIB
islrp_context *islrp_create_default_context( int prealloc ) {
	islrp_context *context = ISLRP_MALLOC( sizeof *context );
	prealloc = prealloc > 0 ? prealloc : 1;
	context->free_rects = ISLRP_MALLOC( sizeof(islrp_rect) * prealloc );
	context->used_rects = ISLRP_MALLOC( sizeof(islrp_rect) * prealloc );
	context->num_free = 0;
	context->num_used = 0;
	context->allocated_free = prealloc;
	context->allocated_used = prealloc;
	return context;
}

void islrp_destroy_default_context( islrp_context *context ) {
	if ( context ) {
		if ( context->free_rects ) {
			ISLRP_FREE( context->free_rects );
			context->free_rects = ISLRP_NULL;
		}
		if ( context->used_rects ) {
			ISLRP_FREE( context->used_rects );
			context->used_rects = ISLRP_NULL;
		}
		ISLRP_FREE( context );
	}
}
#endif

#ifdef ISLRP_NAIVE_MEMMOVE
static void *islrp__memmove( void *dest, const void *src, islrp_size_t count ) {
	islrp_rect *rects_dest = (islrp_rect *) dest;
	islrp_rect *rects_src = (islrp_rect *) src;
	islrp_size_t n = count / sizeof( islrp_rect );
	while( n-- ) {
		*rects_dest = *rects_src;
		rects_dest++;
		rects_src++;
	}
	return dest;
}
#endif

#endif
