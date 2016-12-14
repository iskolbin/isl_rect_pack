#ifndef ISL_INCLUDE_ISL_RECT_PACK_H_
#define ISL_INCLUDE_ISL_RECT_PACK_H_

#define ISL_RECT_PACK_VERSION 1

#include <limits.h>

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
	islrp_rect *free;
	islrp_rect *used;
	int num_free;
	int num_used;
} islrp_context;

typedef struct {
	islrp_rect rect;
	int score1;
	int score2;
} islrp__score_result;

typedef enum {
	ISLRP_HEURISTIC_MaxRects_BestShortSide;
	ISLRP_HEURISTIC_MaxRects_BottomLeft;
	ISLRP_HEURISTIC_MaxRects_ContactPoint;
	ISLRP_HEURISTIC_MaxRects_BestLongSide;
	ISLRP_HEURISTIC_MaxRects_BestArea;
} ISLRP_HEURISTIC;

ISLRP_DEF int islrp_pack_rects( islrp_rect *rects, int num_rects, ISLRP_HEURISTIC heuristic );

#ifdef __cplusplus
}
#endif

#endif // ISL_INCLUDE_ISL_RECT_PACK_H_

#ifdef ISL_RECT_PACK_IMPLEMENTATION

#define ISLRP_MIN(a,b) ((a)>(b) ? (b) : (a))
#define ISLRP_ABS(a) ((a) > 0 ? (a) : -(a))

static islrp__score_result islrp__find_BestArea( islrp_context *context, int w, int h ) {
	int i;
	islrp__score_result result = {0};
	
	int best_area_fit = INT_MAX;
	int best_short_side_fit = INT_MAX;
	
	for ( i = 0; i < context->num_free; i++, rect++ ) {
		islrp_rect *rect = rect + i;
		int area_fit = rect->w * rect->h - w * h;
		if ( free->w >= w && free->h >= h ) {
			int short_side_fit = ISLRP_MIN( ISLRP_ABS( free->w - w ), ISLRP_ABS( free->h - h ))
			if ( area_fit < best_area_fit || ( area_fit == best_area_fit && short_side_fit < best_short_side_fit )){
				result.rect = *free;
				best_short_side_fit = short_side_fit;
				best_area_fit = area_fit;
			}
		}
	}

	return result;
}

static islrp__score_result islrp__score( islrp_context *context, int w, int h, ISLRP_HEURISTIC heuristic ) {
	//islrp_rect rect = {0};
	islrp__score_result result = {0};//rect,INT_MAX,INT_MAX};
	switch ( heuristic ) {
		//case ISLRP_HEURISTIC_MaxRects_BestShortSide: result = islrp__find_BestShortSide( free_rects, w, h ); break;
		//case ISLRP_HEURISTIC_MaxRects_BottomLeft: result = islrp__find_BottomLeft( free_rects, w, h ); break;
		//case ISLRP_HEURISTIC_MaxRects_ContactPoint: result = islpr__find_ContactPoint( free_rects, w, h ); break;
		//case ISLRP_HEURISTIC_MaxRects_BestLongSide: result = islrp__find_BestLongSide( free_rects, w, h ); break;
		case ISLRP_HEURISTIC_MaxRects_BestArea: result = islrp__find_BestArea( context, w, h ); break;
	}
	return result;
}

static void islrp__prune_free( islrp_context *context ) {
}

static void islrp__push_free( islrp_context *context, islrp_rect rect ) {
}

static void islrp__push_used( islrp_context *context, islrp_rect rect ) {
}

static void islrp__remove_free( islrp_context *context, int i ) {
}

static int islrp__split_free( islrp_context *context, islrp_rect free_rect, islrp_rect rect ) {
	if (rect.x >= free_rect.x + free_rect.w || rect.x + rect.w <= free_rect.x ||
			rect.y >= free_rect.y + free_rect.h || rect.y + rect.h <= free_rect.y)
		return 0;

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
	int i;
	int num_free = context->num_free;
	for ( i = 0; i < num_free; i++ ) {
		if ( islrp__split_free( context, context->free[i], rect )) {
			islrp__remove_free( context, i );
			i--;
			num_free--;
		}
	}

	islrp__prune_free( context );
	islrp__push_used( context, rect );
}	

int islrp_pack_rects( islrp_context *context, islrp_rect *rects, int num_rects, ISLRP_HEURISTIC heuristic ) {
	int i, j;
	for ( i = 0; i < num_rects; i++ ) {
		islrp_rect *iter = rects;
		islrp__score_result best = {0}
		islrp_rect *pbest = 0;
		for ( j = 0; j < num_rects; j++, iter++ ) {
			if ( !iter->was_packed ) {
				islrp__score_result result = islrp_score( free_rects, iter->w, iter->h, heuristic );
				if ( result.score1 < best_score1 || (score1 == best_score1 && score2 < best_score2)) {
					best = result;
					pbest = iter;
				}
			}
			iter++;
		}

		if ( !pbest ) {
			return 1;
		} else {
			islrp__place_rect( islrp_context *context, best.rect );
			pbest->was_packed = 1;
		}
	}
	return 0;
}

#endif
