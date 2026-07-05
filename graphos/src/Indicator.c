/* Scintilla source code edit control
 * Indicator.c — C translation of scintilla/src/Indicator.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   enum class State         →  IndicatorState (NORMAL / HOVER)
 *   FlagSet(flags, flag)     →  (flags & flag) != 0
 *   ColourRGBA::FromRGB(v)   →  ColourRGBA_FromRGB(v)
 *   ColourRGBA(fore, alpha)  →  ColourRGBA_WithAlpha(fore, alpha)
 *   PixelAlignOutside/…      →  PixelAlignOutside / PixelAlign_rc
 *   std::vector<Point> pts   →  fixed VLA-sized stack array (max width capped)
 *   std::vector<ColourStop>  →  fixed-size stack array (≤3 stops)
 *   Surface::GradientOptions →  GradientOptions enum
 *   RGBAImage local          →  RGBAImage * (heap, destroyed after draw)
 *   std::size(pts)           →  explicit count variable
 *   std::min / std::round etc→  GEO_MIN / round / floor / ceil from math.h
 */

#include <stdlib.h>
#include <math.h>
#include "Indicator.h"
#include "XPM.h"

/* ── Init ────────────────────────────────────────────────────────────── */

void Indicator_init(Indicator *self) {
    self->sacNormal   = StyleAndColour_make(IndicatorStyle_Plain, ColourRGBA_black);
    self->sacHover    = StyleAndColour_make(IndicatorStyle_Plain, ColourRGBA_black);
    self->under       = 0;
    self->fillAlpha   = 30;
    self->outlineAlpha= 50;
    self->attributes  = IndicFlag_None;
    self->strokeWidth = 1.0;
}

void Indicator_initFull(Indicator *self, IndicatorStyle style, ColourRGBA fore,
                        int under, int fillAlpha, int outlineAlpha) {
    self->sacNormal   = StyleAndColour_make(style, fore);
    self->sacHover    = StyleAndColour_make(style, fore);
    self->under       = under;
    self->fillAlpha   = fillAlpha;
    self->outlineAlpha= outlineAlpha;
    self->attributes  = IndicFlag_None;
    self->strokeWidth = 1.0;
}

void Indicator_SetFlags(Indicator *self, IndicFlag attributes) {
    self->attributes = attributes;
}

/* ── Draw ────────────────────────────────────────────────────────────── */

void Indicator_Draw(const Indicator *self, Surface *surface,
                    PRectangle rc, PRectangle rcLine, PRectangle rcCharacter,
                    IndicatorState state, int value) {

    StyleAndColour sacDraw = self->sacNormal;

    if ((self->attributes & IndicFlag_ValueFore) != 0)
        sacDraw.fore = ColourRGBA_FromRGB(value & IndicValue_Mask);

    if (state == IndicatorState_Hover)
        sacDraw = self->sacHover;

    const int        pixelDivisions = Surface_PixelDivisions(surface);
    const XYPOSITION halfWidth      = self->strokeWidth / 2.0;

    const PRectangle rcAligned          = PixelAlignOutside(rc,     pixelDivisions);
    PRectangle       rcFullHeightAligned = PixelAlignOutside(rcLine, pixelDivisions);
    rcFullHeightAligned.left  = rcAligned.left;
    rcFullHeightAligned.right = rcAligned.right;

    const XYPOSITION ymid  = PixelAlign(PRectangle_Centre(rc).y, pixelDivisions);

    PRectangle rcClip   = rcAligned;
    rcClip.bottom       = rcFullHeightAligned.bottom;

    switch (sacDraw.style) {

    case IndicatorStyle_Squiggle: {
        Surface_SetClip(surface, rcClip);
        const XYPOSITION pitch = 1 + self->strokeWidth;
        /* Worst case: one point per pitch unit across the width */
        const int maxPts = (int)(PRectangle_Width(rcAligned) / pitch) + 4;
        Point *pts = malloc((size_t)maxPts * sizeof(Point));
        if (!pts) { Surface_PopClip(surface); break; }
        int npts = 0;
        XYPOSITION x   = rcAligned.left + halfWidth;
        const XYPOSITION top  = rcAligned.top + halfWidth;
        const XYPOSITION xLast = rcAligned.right + halfWidth;
        XYPOSITION y = 0;
        pts[npts++] = Point_make(x, top + y);
        while (x < xLast && npts < maxPts - 1) {
            x += pitch;
            y  = pitch - y;
            pts[npts++] = Point_make(x, top + y);
        }
        Surface_PolyLine(surface, pts, (size_t)npts, Stroke_make(sacDraw.fore, self->strokeWidth));
        free(pts);
        Surface_PopClip(surface);
        break;
    }

    case IndicatorStyle_SquigglePixmap: {
        const PRectangle rcSquiggle = PixelAlign_rc(rc, 1);
        const int width = GEO_MIN(4000, (int)PRectangle_Width(rcSquiggle));
        RGBAImage *image = RGBAImage_create(width, 3, 1.0f, NULL);
        if (!image) break;
        const unsigned int alphaFull  = 0xFF;
        const unsigned int alphaSide  = 0x2F;
        const unsigned int alphaSide2 = 0x5F;
        for (int x = 0; x < width; x++) {
            if (x % 2) {
                RGBAImage_SetPixel(image, x, 0, ColourRGBA_WithAlpha(sacDraw.fore, alphaSide));
                RGBAImage_SetPixel(image, x, 1, ColourRGBA_WithAlpha(sacDraw.fore, alphaFull));
                RGBAImage_SetPixel(image, x, 2, ColourRGBA_WithAlpha(sacDraw.fore, alphaSide));
            } else {
                RGBAImage_SetPixel(image, x, (x%4) ? 0 : 2, ColourRGBA_WithAlpha(sacDraw.fore, alphaFull));
                RGBAImage_SetPixel(image, x, 1,              ColourRGBA_WithAlpha(sacDraw.fore, alphaSide2));
            }
        }
        Surface_DrawRGBAImage(surface, rcSquiggle,
                               RGBAImage_GetWidth(image), RGBAImage_GetHeight(image),
                               RGBAImage_Pixels(image));
        RGBAImage_destroy(image);
        break;
    }

    case IndicatorStyle_SquiggleLow: {
        const XYPOSITION pitch  = 2 + self->strokeWidth;
        const int maxPts = (int)(PRectangle_Width(rcAligned) / pitch) * 2 + 8;
        Point *pts = malloc((size_t)maxPts * sizeof(Point));
        if (!pts) break;
        int npts = 0;
        const XYPOSITION top = rcAligned.top + halfWidth;
        int    iy = 0;
        XYPOSITION x = round(rcAligned.left) + halfWidth;
        pts[npts++] = Point_make(x, top + iy);
        x += pitch;
        while (x < rcAligned.right && npts < maxPts - 2) {
            pts[npts++] = Point_make(x - 1, top + iy);
            iy = 1 - iy;
            pts[npts++] = Point_make(x,     top + iy);
            x += pitch;
        }
        pts[npts++] = Point_make(rcAligned.right, top + iy);
        Surface_PolyLine(surface, pts, (size_t)npts, Stroke_make(sacDraw.fore, self->strokeWidth));
        free(pts);
        break;
    }

    case IndicatorStyle_TT: {
        Surface_SetClip(surface, rcClip);
        const XYPOSITION yLine = ymid;
        XYPOSITION x = rcAligned.left + 5.0;
        const XYPOSITION pitch = 4 + self->strokeWidth;
        while (x < rc.right + pitch) {
            const PRectangle line = PRectangle_make(x-pitch, yLine, x, yLine + self->strokeWidth);
            Surface_FillRectangle(surface, line, sacDraw.fore);
            const PRectangle tail = PRectangle_make(x - 2 - self->strokeWidth,
                                                     yLine + self->strokeWidth,
                                                     x - 2,
                                                     yLine + self->strokeWidth * 2);
            Surface_FillRectangle(surface, tail, sacDraw.fore);
            x++;
            x += pitch;
        }
        Surface_PopClip(surface);
        break;
    }

    case IndicatorStyle_Diagonal: {
        Surface_SetClip(surface, rcClip);
        XYPOSITION x = rcAligned.left + halfWidth;
        const XYPOSITION top   = rcAligned.top + halfWidth;
        const XYPOSITION pitch = 3 + self->strokeWidth;
        while (x < rc.right) {
            const XYPOSITION endX = x + 3;
            const XYPOSITION endY = top - 1;
            Surface_LineDraw(surface, Point_make(x, top + 2), Point_make(endX, endY),
                              Stroke_make(sacDraw.fore, self->strokeWidth));
            x += pitch;
        }
        Surface_PopClip(surface);
        break;
    }

    case IndicatorStyle_Strike: {
        const XYPOSITION yStrike = round(PRectangle_Centre(rcLine).y);
        const PRectangle rcStrike = PRectangle_make(rcAligned.left, yStrike,
                                                     rcAligned.right, yStrike + self->strokeWidth);
        Surface_FillRectangle(surface, rcStrike, sacDraw.fore);
        break;
    }

    case IndicatorStyle_Hidden:
    case IndicatorStyle_TextFore:
        break;

    case IndicatorStyle_Box: {
        PRectangle rcBox  = rcFullHeightAligned;
        rcBox.top        += 1.0;
        rcBox.bottom      = ymid + 1.0;
        Surface_RectangleFrame(surface, rcBox,
                                Stroke_make(ColourRGBA_WithAlpha(sacDraw.fore, (unsigned int)self->outlineAlpha),
                                            self->strokeWidth));
        break;
    }

    case IndicatorStyle_RoundBox:
    case IndicatorStyle_StraightBox:
    case IndicatorStyle_FullBox: {
        PRectangle rcBox = rcFullHeightAligned;
        if (sacDraw.style != IndicatorStyle_FullBox)
            rcBox.top += 1;
        Surface_AlphaRectangle(surface, rcBox,
                                (sacDraw.style == IndicatorStyle_RoundBox) ? 1.0 : 0.0,
                                FillStroke_make2(ColourRGBA_WithAlpha(sacDraw.fore, (unsigned int)self->fillAlpha),
                                                 ColourRGBA_WithAlpha(sacDraw.fore, (unsigned int)self->outlineAlpha),
                                                 self->strokeWidth));
        break;
    }

    case IndicatorStyle_Gradient:
    case IndicatorStyle_GradientCentre: {
        PRectangle rcBox = rcFullHeightAligned;
        rcBox.top += 1;
        const ColourRGBA start = ColourRGBA_WithAlpha(sacDraw.fore, (unsigned int)self->fillAlpha);
        const ColourRGBA end   = ColourRGBA_WithAlpha(sacDraw.fore, 0);
        ColourStop stops[3];
        int nstops = 0;
        if (sacDraw.style == IndicatorStyle_Gradient) {
            stops[nstops++] = ColourStop_make(0.0, start);
            stops[nstops++] = ColourStop_make(1.0, end);
        } else {
            stops[nstops++] = ColourStop_make(0.0, end);
            stops[nstops++] = ColourStop_make(0.5, start);
            stops[nstops++] = ColourStop_make(1.0, end);
        }
        Surface_GradientRectangle(surface, rcBox, stops, nstops, GRADIENT_TOP_TO_BOTTOM);
        break;
    }

    case IndicatorStyle_DotBox: {
        PRectangle rcBox = rcFullHeightAligned;
        rcBox.top += 1;
        const int width  = GEO_MIN((int)PRectangle_Width(rcBox),  4000);
        const int height = (int)PRectangle_Height(rcBox);
        RGBAImage *image = RGBAImage_create(width, height, 1.0f, NULL);
        if (!image) break;
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y += height - 1) {
                const unsigned int alpha = (unsigned int)(((x+y)%2) ? self->outlineAlpha : self->fillAlpha);
                RGBAImage_SetPixel(image, x, y, ColourRGBA_WithAlpha(sacDraw.fore, alpha));
            }
        }
        for (int y = 1; y < height; y++) {
            for (int x = 0; x < width; x += width - 1) {
                const unsigned int alpha = (unsigned int)(((x+y)%2) ? self->outlineAlpha : self->fillAlpha);
                RGBAImage_SetPixel(image, x, y, ColourRGBA_WithAlpha(sacDraw.fore, alpha));
            }
        }
        Surface_DrawRGBAImage(surface, rcBox,
                               RGBAImage_GetWidth(image), RGBAImage_GetHeight(image),
                               RGBAImage_Pixels(image));
        RGBAImage_destroy(image);
        break;
    }

    case IndicatorStyle_Dash: {
        const XYPOSITION widthDash = 3 + round(self->strokeWidth);
        XYPOSITION x = floor(rc.left);
        while (x < rc.right) {
            const PRectangle rcDash = PRectangle_make(x, ymid,
                                                       x + widthDash, ymid + round(self->strokeWidth));
            Surface_FillRectangle(surface, rcDash, sacDraw.fore);
            x += 3 + widthDash;
        }
        break;
    }

    case IndicatorStyle_Dots: {
        const XYPOSITION widthDot = round(self->strokeWidth);
        XYPOSITION x = floor(rc.left);
        while (x < rc.right) {
            const PRectangle rcDot = PRectangle_make(x, ymid, x + widthDot, ymid + widthDot);
            Surface_FillRectangle(surface, rcDot, sacDraw.fore);
            x += widthDot * 2;
        }
        break;
    }

    case IndicatorStyle_CompositionThick: {
        const PRectangle rcComp = PRectangle_make(rc.left+1, rcLine.bottom-2, rc.right-1, rcLine.bottom);
        Surface_FillRectangle(surface, rcComp, ColourRGBA_WithAlpha(sacDraw.fore, (unsigned int)self->outlineAlpha));
        break;
    }

    case IndicatorStyle_CompositionThin: {
        const PRectangle rcComp = PRectangle_make(rc.left+1, rcLine.bottom-2, rc.right-1, rcLine.bottom-1);
        Surface_FillRectangle(surface, rcComp, sacDraw.fore);
        break;
    }

    case IndicatorStyle_Point:
    case IndicatorStyle_PointCharacter:
        if (PRectangle_Width(rcCharacter) >= 0.1) {
            const XYPOSITION pixelHeight = floor(PRectangle_Height(rc));
            const XYPOSITION x = (sacDraw.style == IndicatorStyle_Point)
                                  ? rcCharacter.left
                                  : (rcCharacter.right + rcCharacter.left) / 2;
            const XYPOSITION ix = round(x) + 0.5;
            const XYPOSITION iy = ceil(rc.bottom) + 0.5;
            const Point pts[3] = {
                Point_make(ix - pixelHeight, iy),
                Point_make(ix + pixelHeight, iy),
                Point_make(ix,               iy - pixelHeight)
            };
            Surface_Polygon(surface, pts, 3, FillStroke_make1(sacDraw.fore, 1.0));
        }
        break;

    case IndicatorStyle_PointTop:
        if (PRectangle_Width(rcCharacter) >= 0.1) {
            const XYPOSITION pixelHeight = floor(PRectangle_Height(rc));
            const XYPOSITION x  = rcCharacter.left;
            const XYPOSITION ix = round(x) + 0.5;
            const XYPOSITION iy = floor(rcLine.top) - 0.5;
            const Point pts[3] = {
                Point_make(ix - pixelHeight, iy),
                Point_make(ix + pixelHeight, iy),
                Point_make(ix,               iy + pixelHeight)
            };
            Surface_Polygon(surface, pts, 3, FillStroke_make1(sacDraw.fore, 1.0));
        }
        break;

    default:
        /* Plain, ExplorerLink, or unknown */
        Surface_FillRectangle(surface,
            PRectangle_make(rcAligned.left, ymid, rcAligned.right, ymid + round(self->strokeWidth)),
            sacDraw.fore);
        break;
    }
}
