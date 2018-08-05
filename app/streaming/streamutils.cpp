#include "streamutils.h"

void StreamUtils::scaleSourceToDestinationSurface(SDL_Rect* src, SDL_Rect* dst)
{
    int dstH = dst->w * src->h / src->w;
    int dstW = dst->h * src->w / src->h;

    if (dstH > dst->h) {
        dst->y = 0;
        dst->x = (dst->w - dstW) / 2;
        dst->w = dstW;
        SDL_assert(dst->w * src->h / src->w <= dst->h);
    }
    else {
        dst->x = 0;
        dst->y = (dst->h - dstH) / 2;
        dst->h = dstH;
        SDL_assert(dst->h * src->w / src->h <= dst->w);
    }
}
