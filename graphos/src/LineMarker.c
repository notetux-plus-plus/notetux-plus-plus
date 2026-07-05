/* Scintilla source code edit control
 * LineMarker.c — C translation of scintilla/src/LineMarker.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   std::unique_ptr<XPM/RGBAImage>  →  raw pointers, freed in destroy/copy
 *   copy ctor / operator=           →  LineMarker_copy()
 *   anonymous-namespace helpers     →  static file-scope functions
 *   std::vector<Point> + transform  →  fixed stack array + manual shift loop
 *   std::lround / std::floor etc    →  lround / floor from math.h
 *   std::min(a,b)                   →  GEO_MIN(a,b)
 *   MarkerSymbol::Character         →  MarkerSymbol_Character (int offset)
 *   UTF8MaxBytes                    →  4 (UTF-8 max is 4 continuation bytes + NUL = 5)
 *   string_view text to Surface     →  const char * + strlen()
 *   enum Expansion / Shape          →  file-private typedef enum
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "LineMarker.h"
#include "UniConversion.h"

/* ── Init / destroy / copy ──────────────────────────────────────────���─── */

void LineMarker_init(LineMarker *self) {
    self->markType    = MarkerSymbol_Circle;
    self->fore        = ColourRGBA_black;
    self->back        = ColourRGBA_white;
    self->backSelected= ColourRGBA_make(COLOUR_MAX_BYTE, 0, 0, COLOUR_MAX_BYTE);
    self->layer       = Layer_Base;
    self->alpha       = Alpha_NoAlpha;
    self->strokeWidth = 1.0;
    self->pxpm        = NULL;
    self->image       = NULL;
    self->customDraw  = NULL;
}

void LineMarker_destroy(LineMarker *self) {
    if (self->pxpm) {
        XPM_destroy(self->pxpm);
        free(self->pxpm);
        self->pxpm = NULL;
    }
    if (self->image) {
        RGBAImage_destroy(self->image);
        self->image = NULL;
    }
}

void LineMarker_copy(LineMarker *dst, const LineMarker *src) {
    LineMarker_destroy(dst);
    dst->markType     = src->markType;
    dst->fore         = src->fore;
    dst->back         = src->back;
    dst->backSelected = src->backSelected;
    dst->strokeWidth  = src->strokeWidth;
    dst->layer        = src->layer;
    dst->alpha        = src->alpha;
    dst->customDraw   = src->customDraw;

    dst->pxpm = NULL;
    if (src->pxpm) {
        dst->pxpm = malloc(sizeof(XPM));
        if (dst->pxpm) {
            dst->pxpm->pixels = NULL; /* init before copy */
            XPM_init_lines(dst->pxpm, NULL);
            /* Deep copy: re-parse from existing pixel data via PixelAt */
            dst->pxpm->height         = src->pxpm->height;
            dst->pxpm->width          = src->pxpm->width;
            dst->pxpm->nColours       = src->pxpm->nColours;
            dst->pxpm->codeTransparent= src->pxpm->codeTransparent;
            memcpy(dst->pxpm->colourCodeTable, src->pxpm->colourCodeTable,
                   sizeof(src->pxpm->colourCodeTable));
            if (src->pxpm->pixels) {
                const size_t n = (size_t)(src->pxpm->width * src->pxpm->height);
                dst->pxpm->pixels = malloc(n);
                if (dst->pxpm->pixels)
                    memcpy(dst->pxpm->pixels, src->pxpm->pixels, n);
            }
        }
    }

    dst->image = NULL;
    if (src->image) {
        dst->image = RGBAImage_create(src->image->width, src->image->height,
                                       src->image->scale, src->image->pixelBytes);
    }
}

/* ── BackWithAlpha ────────────────────────────────────────────────────── */

ColourRGBA LineMarker_BackWithAlpha(const LineMarker *self) {
    return ColourRGBA_WithAlpha(self->back, (unsigned int)self->alpha);
}

/* ── SetXPM / SetRGBAImage ────────────────────────────────────────────── */

void LineMarker_SetXPM(LineMarker *self, const char *textForm) {
    LineMarker_destroy(self);
    self->pxpm = malloc(sizeof(XPM));
    if (self->pxpm) {
        self->pxpm->pixels = NULL;
        XPM_init_text(self->pxpm, textForm);
    }
    self->markType = MarkerSymbol_Pixmap;
}

void LineMarker_SetXPMLines(LineMarker *self, const char *const *linesForm) {
    LineMarker_destroy(self);
    self->pxpm = malloc(sizeof(XPM));
    if (self->pxpm) {
        self->pxpm->pixels = NULL;
        XPM_init_lines(self->pxpm, linesForm);
    }
    self->markType = MarkerSymbol_Pixmap;
}

void LineMarker_SetRGBAImage(LineMarker *self, Point sizeRGBAImage, float scale,
                              const unsigned char *pixelsRGBAImage) {
    if (self->image) { RGBAImage_destroy(self->image); self->image = NULL; }
    self->image = RGBAImage_create((int)sizeRGBAImage.x, (int)sizeRGBAImage.y,
                                    scale, pixelsRGBAImage);
    self->markType = MarkerSymbol_RgbaImage;
}

/* ── File-private drawing helpers ─────────────────────────────────────── */

typedef enum { Expansion_Minus, Expansion_Plus } Expansion;
typedef enum { Shape_Square, Shape_Circle }       DrawShape;

static void DrawSymbol(Surface *surface, DrawShape shape, Expansion expansion,
                        PRectangle rcSymbol, XYPOSITION widthStroke,
                        ColourRGBA colourFill, ColourRGBA colourFrame,
                        ColourRGBA colourFrameRight, ColourRGBA colourExpansion) {

    const FillStroke fs      = FillStroke_make2(colourFill, colourFrame,      widthStroke);
    const FillStroke fsRight = FillStroke_make2(colourFill, colourFrameRight, widthStroke);

    /* Left half */
    const PRectangle rcLeft = Side(rcSymbol, EDGE_LEFT,
                                    (PRectangle_Width(rcSymbol) + widthStroke) / 2.0);
    Surface_SetClip(surface, rcLeft);
    if (shape == Shape_Square)
        Surface_RectangleDraw(surface, rcSymbol, fs);
    else
        Surface_Ellipse(surface, rcSymbol, fs);
    Surface_PopClip(surface);

    /* Right half */
    const PRectangle rcRight = Side(rcSymbol, EDGE_RIGHT,
                                     (PRectangle_Width(rcSymbol) - widthStroke) / 2.0);
    Surface_SetClip(surface, rcRight);
    if (shape == Shape_Square)
        Surface_RectangleDraw(surface, rcSymbol, fsRight);
    else
        Surface_Ellipse(surface, rcSymbol, fsRight);
    Surface_PopClip(surface);

    /* Plus/minus bar */
    const PRectangle rcPM      = PRectangle_InsetScalar(rcSymbol, widthStroke + 1.0);
    const XYPOSITION armWidth  = (PRectangle_Width(rcPM) - widthStroke) / 2.0;
    const XYPOSITION top       = rcPM.top + armWidth;
    const PRectangle rcH       = PRectangle_make(rcPM.left, top,
                                                  rcPM.right, top + widthStroke);
    Surface_FillRectangle(surface, rcH, colourExpansion);

    if (expansion == Expansion_Plus) {
        const XYPOSITION left  = rcPM.left + armWidth;
        const PRectangle rcV   = PRectangle_make(left, rcPM.top,
                                                  left + widthStroke, rcPM.bottom);
        Surface_FillRectangle(surface, rcV, colourExpansion);
    }
}

static void DrawTail(Surface *surface, XYPOSITION leftLine, XYPOSITION rightTail,
                      XYPOSITION centreY, XYPOSITION widthSymbolStroke, ColourRGBA fill) {
    const XYPOSITION slopeLength = 2.0 + widthSymbolStroke;
    const XYPOSITION strokeTop   = centreY + slopeLength;
    const XYPOSITION halfWidth   = widthSymbolStroke / 2.0;
    const XYPOSITION strokeMiddle= strokeTop + halfWidth;
    const Point lines[3] = {
        Point_make(rightTail,                                    strokeMiddle),
        Point_make(leftLine + halfWidth + slopeLength,           strokeMiddle),
        Point_make(leftLine + widthSymbolStroke / 2.0,           centreY + halfWidth),
    };
    Surface_PolyLine(surface, lines, 3, Stroke_make(fill, widthSymbolStroke));
}

/* Shift all polygon points by strokeWidth/2 then fill+stroke */
static void LM_AlignedPolygon(const LineMarker *self, Surface *surface,
                                const Point *pts, size_t npts) {
    const XYPOSITION move = self->strokeWidth / 2.0;
    Point shifted[16];   /* largest use: Plus = 12 pts */
    const size_t n = (npts < 16) ? npts : 16;
    for (size_t i = 0; i < n; i++)
        shifted[i] = Point_make(pts[i].x + move, pts[i].y + move);
    Surface_Polygon(surface, shifted, n,
                     FillStroke_make2(self->back, self->fore, self->strokeWidth));
}

/* ── DrawFoldingMark ────────────────────────────────────────────��─────── */

void LineMarker_DrawFoldingMark(const LineMarker *self, Surface *surface,
                                 PRectangle rcWhole, FoldPart part) {
    ColourRGBA colourHead = self->back;
    ColourRGBA colourBody = self->back;
    ColourRGBA colourTail = self->back;

    switch (part) {
    case FoldPart_head:
    case FoldPart_headWithTail:
        colourHead = self->backSelected;
        colourTail = self->backSelected;
        break;
    case FoldPart_body:
        colourHead = self->backSelected;
        colourBody = self->backSelected;
        break;
    case FoldPart_tail:
        colourBody = self->backSelected;
        colourTail = self->backSelected;
        break;
    default:
        break;
    }

    const int        pixelDivisions = Surface_PixelDivisions(surface);
    const XYPOSITION minDimension   = floor(GEO_MIN(PRectangle_Width(rcWhole),
                                                      PRectangle_Height(rcWhole) - 2)) - 1;
    const XYPOSITION widthStroke    = PixelAlignFloor(
                                         GEO_MIN(self->strokeWidth, minDimension / 5.0),
                                         pixelDivisions);

    /* Parity trick: symbol width matches stroke parity for centred +/- */
    const long roundMin    = lround(minDimension  * pixelDivisions);
    const long roundStroke = lround(widthStroke   * pixelDivisions);
    const XYPOSITION widthSymbol = ((roundMin % 2) == (roundStroke % 2))
                                    ? minDimension
                                    : minDimension - (1.0 / (XYPOSITION)pixelDivisions);

    const Point centre     = PixelAlign_pt(PRectangle_Centre(rcWhole), pixelDivisions);
    const XYPOSITION halfSymbol = round(widthSymbol / 2);
    const Point topLeft    = Point_make(centre.x - halfSymbol, centre.y - halfSymbol);
    const PRectangle rcSymbol = PRectangle_make(topLeft.x, topLeft.y,
                                                 topLeft.x + widthSymbol,
                                                 topLeft.y + widthSymbol);
    const XYPOSITION leftLine  = PRectangle_Centre(rcSymbol).x - widthStroke / 2.0;
    const XYPOSITION rightLine = leftLine + widthStroke;

    const PRectangle rcVLine      = PRectangle_make(leftLine, rcWhole.top,
                                                     rightLine, rcWhole.bottom);
    const PRectangle rcAboveSymbol= Clamp(rcVLine, EDGE_BOTTOM, rcSymbol.top);
    const PRectangle rcBelowSymbol= Clamp(rcVLine, EDGE_TOP,    rcSymbol.bottom);
    const PRectangle rcStick      = PRectangle_make(rcVLine.right,
                                                     centre.y + 1.0 - widthStroke,
                                                     rcWhole.right - 1,
                                                     centre.y + 1.0);

    switch (self->markType) {

    case MarkerSymbol_VLine:
        Surface_FillRectangle(surface, rcVLine, colourBody);
        break;

    case MarkerSymbol_LCorner:
        Surface_FillRectangle(surface, Clamp(rcVLine, EDGE_BOTTOM, centre.y + 1.0), colourTail);
        Surface_FillRectangle(surface, rcStick, colourTail);
        break;

    case MarkerSymbol_TCorner:
        Surface_FillRectangle(surface, Clamp(rcVLine, EDGE_BOTTOM, centre.y + 1.0), colourBody);
        Surface_FillRectangle(surface, Clamp(rcVLine, EDGE_TOP,    centre.y + 1.0), colourHead);
        Surface_FillRectangle(surface, rcStick, colourTail);
        break;

    case MarkerSymbol_LCornerCurve:
        Surface_FillRectangle(surface, Clamp(rcVLine, EDGE_BOTTOM, centre.y), colourTail);
        DrawTail(surface, leftLine, rcWhole.right - 1.0, centre.y - widthStroke,
                 widthStroke, colourTail);
        break;

    case MarkerSymbol_TCornerCurve:
        Surface_FillRectangle(surface, Clamp(rcVLine, EDGE_BOTTOM, centre.y), colourBody);
        Surface_FillRectangle(surface, Clamp(rcVLine, EDGE_TOP,    centre.y), colourHead);
        DrawTail(surface, leftLine, rcWhole.right - 1.0, centre.y - widthStroke,
                 widthStroke, colourTail);
        break;

    case MarkerSymbol_BoxPlus:
        DrawSymbol(surface, Shape_Square, Expansion_Plus, rcSymbol, widthStroke,
                   colourHead, self->fore, self->fore, self->fore);
        break;

    case MarkerSymbol_BoxPlusConnected: {
        const ColourRGBA colourBelow = (part == FoldPart_headWithTail) ? colourTail : colourBody;
        Surface_FillRectangle(surface, rcBelowSymbol, colourBelow);
        Surface_FillRectangle(surface, rcAboveSymbol, colourBody);
        DrawSymbol(surface, Shape_Square, Expansion_Plus, rcSymbol, widthStroke,
                   colourHead, self->fore, self->fore, self->fore);
        break;
    }

    case MarkerSymbol_BoxMinus:
        Surface_FillRectangle(surface, rcBelowSymbol, colourHead);
        DrawSymbol(surface, Shape_Square, Expansion_Minus, rcSymbol, widthStroke,
                   colourHead, self->fore, self->fore, self->fore);
        break;

    case MarkerSymbol_BoxMinusConnected:
        Surface_FillRectangle(surface, rcBelowSymbol, colourHead);
        Surface_FillRectangle(surface, rcAboveSymbol, colourBody);
        DrawSymbol(surface, Shape_Square, Expansion_Minus, rcSymbol, widthStroke,
                   colourHead, self->fore, self->fore, self->fore);
        break;

    case MarkerSymbol_CirclePlus:
        DrawSymbol(surface, Shape_Circle, Expansion_Plus, rcSymbol, widthStroke,
                   self->fore, colourHead, colourHead, colourTail);
        break;

    case MarkerSymbol_CirclePlusConnected: {
        const ColourRGBA colourBelow = (part == FoldPart_headWithTail) ? colourTail : colourBody;
        Surface_FillRectangle(surface, rcBelowSymbol, colourBelow);
        Surface_FillRectangle(surface, rcAboveSymbol, colourBody);
        const ColourRGBA colourRight = (part == FoldPart_body) ? colourTail : colourHead;
        DrawSymbol(surface, Shape_Circle, Expansion_Plus, rcSymbol, widthStroke,
                   self->fore, colourHead, colourRight, colourTail);
        break;
    }

    case MarkerSymbol_CircleMinus:
        Surface_FillRectangle(surface, rcBelowSymbol, colourHead);
        DrawSymbol(surface, Shape_Circle, Expansion_Minus, rcSymbol, widthStroke,
                   self->fore, colourHead, colourHead, colourTail);
        break;

    case MarkerSymbol_CircleMinusConnected: {
        Surface_FillRectangle(surface, rcBelowSymbol, colourHead);
        Surface_FillRectangle(surface, rcAboveSymbol, colourBody);
        const ColourRGBA colourRight = (part == FoldPart_body) ? colourTail : colourHead;
        DrawSymbol(surface, Shape_Circle, Expansion_Minus, rcSymbol, widthStroke,
                   self->fore, colourHead, colourRight, colourTail);
        break;
    }

    default:
        break;
    }
}

/* ── Draw ─────────────────────────────────────────────────────────────── */

void LineMarker_Draw(const LineMarker *self, Surface *surface,
                      PRectangle rcWhole, const Font *fontForCharacter,
                      FoldPart part, MarginType marginStyle) {

    if (self->customDraw) {
        self->customDraw(surface, rcWhole, fontForCharacter, (int)part, marginStyle, self);
        return;
    }

    if (self->markType == MarkerSymbol_Pixmap && self->pxpm) {
        XPM_Draw(self->pxpm, surface, rcWhole);
        return;
    }

    if (self->markType == MarkerSymbol_RgbaImage && self->image) {
        PRectangle rcImage;
        const float sh = RGBAImage_GetScaledHeight(self->image);
        const float sw = RGBAImage_GetScaledWidth (self->image);
        rcImage.top    = ((rcWhole.top  + rcWhole.bottom) - sh) / 2;
        rcImage.bottom = rcImage.top  + sh;
        rcImage.left   = ((rcWhole.left + rcWhole.right)  - sw) / 2;
        rcImage.right  = rcImage.left + sw;
        Surface_DrawRGBAImage(surface, rcImage,
                               RGBAImage_GetWidth(self->image),
                               RGBAImage_GetHeight(self->image),
                               RGBAImage_Pixels(self->image));
        return;
    }

    if (self->markType >= MarkerSymbol_VLine &&
        self->markType <= MarkerSymbol_CircleMinusConnected) {
        LineMarker_DrawFoldingMark(self, surface, rcWhole, part);
        return;
    }

    /* Standard shapes */
    const PRectangle rc = PRectangle_make(rcWhole.left, rcWhole.top + 1,
                                           rcWhole.right, rcWhole.bottom - 1);
    const XYPOSITION minDim   = GEO_MIN(PRectangle_Width(rcWhole),
                                         PRectangle_Height(rcWhole) - 2) - 1;
    const Point      centre   = PRectangle_Centre(rcWhole);
    XYPOSITION       centreX  = floor(centre.x);
    const XYPOSITION centreY  = floor(centre.y);
    const XYPOSITION dimOn2   = floor(minDim / 2);
    const XYPOSITION dimOn4   = floor(minDim / 4);
    const XYPOSITION armSize  = dimOn2 - 2;

    if (marginStyle == MarginType_Number || marginStyle == MarginType_Text ||
        marginStyle == MarginType_RText)
        centreX = rcWhole.left + dimOn2 + 1;

    switch (self->markType) {

    case MarkerSymbol_RoundRect: {
        PRectangle rcRounded = rc;
        rcRounded.left  += 1;
        rcRounded.right -= 1;
        Surface_RoundedRectangle(surface, rcRounded,
                                  FillStroke_make2(self->back, self->fore, self->strokeWidth));
        break;
    }

    case MarkerSymbol_Circle: {
        const PRectangle rcCircle = PRectangle_make(centreX - dimOn2, centreY - dimOn2,
                                                     centreX + dimOn2, centreY + dimOn2);
        Surface_Ellipse(surface, rcCircle,
                         FillStroke_make2(self->back, self->fore, self->strokeWidth));
        break;
    }

    case MarkerSymbol_Arrow: {
        const Point pts[3] = {
            Point_make(centreX - dimOn4,            centreY - dimOn2),
            Point_make(centreX - dimOn4,            centreY + dimOn2),
            Point_make(centreX + dimOn2 - dimOn4,   centreY),
        };
        LM_AlignedPolygon(self, surface, pts, 3);
        break;
    }

    case MarkerSymbol_ArrowDown: {
        const Point pts[3] = {
            Point_make(centreX - dimOn2,            centreY - dimOn4),
            Point_make(centreX + dimOn2,            centreY - dimOn4),
            Point_make(centreX,                     centreY + dimOn2 - dimOn4),
        };
        LM_AlignedPolygon(self, surface, pts, 3);
        break;
    }

    case MarkerSymbol_Plus: {
        const Point pts[12] = {
            Point_make(centreX - armSize, centreY - 1),
            Point_make(centreX - 1,       centreY - 1),
            Point_make(centreX - 1,       centreY - armSize),
            Point_make(centreX + 1,       centreY - armSize),
            Point_make(centreX + 1,       centreY - 1),
            Point_make(centreX + armSize, centreY - 1),
            Point_make(centreX + armSize, centreY + 1),
            Point_make(centreX + 1,       centreY + 1),
            Point_make(centreX + 1,       centreY + armSize),
            Point_make(centreX - 1,       centreY + armSize),
            Point_make(centreX - 1,       centreY + 1),
            Point_make(centreX - armSize, centreY + 1),
        };
        LM_AlignedPolygon(self, surface, pts, 12);
        break;
    }

    case MarkerSymbol_Minus: {
        const Point pts[4] = {
            Point_make(centreX - armSize, centreY - 1),
            Point_make(centreX + armSize, centreY - 1),
            Point_make(centreX + armSize, centreY + 1),
            Point_make(centreX - armSize, centreY + 1),
        };
        LM_AlignedPolygon(self, surface, pts, 4);
        break;
    }

    case MarkerSymbol_SmallRect: {
        PRectangle rcSmall;
        rcSmall.left   = rc.left   + 1;
        rcSmall.top    = rc.top    + 2;
        rcSmall.right  = rc.right  - 1;
        rcSmall.bottom = rc.bottom - 2;
        Surface_RectangleDraw(surface, rcSmall,
                               FillStroke_make2(self->back, self->fore, self->strokeWidth));
        break;
    }

    case MarkerSymbol_Empty:
    case MarkerSymbol_Background:
    case MarkerSymbol_Underline:
    case MarkerSymbol_Available:
        break;

    case MarkerSymbol_DotDotDot: {
        const XYPOSITION pitchDots = 5.0;
        XYPOSITION xBlob = floor(centreX - (pitchDots + 1));
        for (int b = 0; b < 3; b++) {
            const PRectangle rcBlob = PRectangle_make(xBlob, rc.bottom - 4,
                                                       xBlob + 2, rc.bottom - 2);
            Surface_FillRectangle(surface, rcBlob, self->fore);
            xBlob += pitchDots;
        }
        break;
    }

    case MarkerSymbol_Arrows: {
        XYPOSITION right      = centreX - 4.0 + self->strokeWidth / 2.0;
        const XYPOSITION midY = centreY + self->strokeWidth / 2.0;
        const XYPOSITION armLength = round(dimOn2 - self->strokeWidth);
        for (int b = 0; b < 3; b++) {
            const Point pts[3] = {
                Point_make(right - armLength, midY - armLength),
                Point_make(right,             midY),
                Point_make(right - armLength, midY + armLength),
            };
            Surface_PolyLine(surface, pts, 3, Stroke_make(self->fore, self->strokeWidth));
            right += self->strokeWidth + 3.0;
        }
        break;
    }

    case MarkerSymbol_ShortArrow: {
        const Point pts[8] = {
            Point_make(centreX,            centreY + dimOn2),
            Point_make(centreX + dimOn2,   centreY),
            Point_make(centreX,            centreY - dimOn2),
            Point_make(centreX,            centreY - dimOn4),
            Point_make(centreX - dimOn4,   centreY - dimOn4),
            Point_make(centreX - dimOn4,   centreY + dimOn4),
            Point_make(centreX,            centreY + dimOn4),
            Point_make(centreX,            centreY + dimOn2),
        };
        LM_AlignedPolygon(self, surface, pts, 8);
        break;
    }

    case MarkerSymbol_FullRect:
        Surface_FillRectangle(surface, rcWhole, self->back);
        break;

    case MarkerSymbol_LeftRect: {
        PRectangle rcLeft = rcWhole;
        rcLeft.right = rcLeft.left + 4;
        Surface_FillRectangle(surface, rcLeft, self->back);
        break;
    }

    case MarkerSymbol_Bar: {
        const XYPOSITION continueLength = 5.0;
        PRectangle rcBar = rcWhole;
        const XYPOSITION widthBar = ceil(PRectangle_Width(rcWhole) / 3.0);
        rcBar.left  = centreX - floor(widthBar / 2.0);
        rcBar.right = rcBar.left + widthBar;
        Surface_SetClip(surface, rcWhole);
        switch (part) {
        case FoldPart_headWithTail:
            Surface_RectangleDraw(surface, rcBar,
                                   FillStroke_make2(self->back, self->fore, self->strokeWidth));
            break;
        case FoldPart_head:
            rcBar.bottom += continueLength;
            Surface_RectangleDraw(surface, rcBar,
                                   FillStroke_make2(self->back, self->fore, self->strokeWidth));
            break;
        case FoldPart_tail:
            rcBar.top -= continueLength;
            Surface_RectangleDraw(surface, rcBar,
                                   FillStroke_make2(self->back, self->fore, self->strokeWidth));
            break;
        case FoldPart_body:
            rcBar.top    -= continueLength;
            rcBar.bottom += continueLength;
            Surface_RectangleDraw(surface, rcBar,
                                   FillStroke_make2(self->back, self->fore, self->strokeWidth));
            break;
        default:
            break;
        }
        Surface_PopClip(surface);
        break;
    }

    case MarkerSymbol_Bookmark: {
        const XYPOSITION halfHeight = floor(minDim / 3);
        const Point pts[5] = {
            Point_make(rcWhole.left,                                   centreY - halfHeight),
            Point_make(rcWhole.right - self->strokeWidth - 2,          centreY - halfHeight),
            Point_make(rcWhole.right - self->strokeWidth - 2 - halfHeight, centreY),
            Point_make(rcWhole.right - self->strokeWidth - 2,          centreY + halfHeight),
            Point_make(rcWhole.left,                                   centreY + halfHeight),
        };
        LM_AlignedPolygon(self, surface, pts, 5);
        break;
    }

    case MarkerSymbol_VerticalBookmark: {
        const XYPOSITION halfWidth = floor(minDim / 3);
        const Point pts[5] = {
            Point_make(centreX - halfWidth, centreY - dimOn2),
            Point_make(centreX + halfWidth, centreY - dimOn2),
            Point_make(centreX + halfWidth, centreY + dimOn2),
            Point_make(centreX,             centreY + dimOn2 - halfWidth),
            Point_make(centreX - halfWidth, centreY + dimOn2),
        };
        LM_AlignedPolygon(self, surface, pts, 5);
        break;
    }

    default:
        if (self->markType >= MarkerSymbol_Character) {
            /* Draw a Unicode character centred in the margin */
            char character[5] = {0};   /* max 4 UTF-8 bytes + NUL */
            const int uch = (int)self->markType - (int)MarkerSymbol_Character;
            UTF8FromUTF32Character(uch, character);
            const size_t len = strlen(character);
            const XYPOSITION width = Surface_WidthTextUTF8(surface,
                                         (Font *)fontForCharacter, character, len);
            PRectangle rcText = rc;
            rcText.left  += (PRectangle_Width(rc) - width) / 2;
            rcText.right  = rcText.left + width;
            Surface_DrawTextNoClipUTF8(surface, rcText, (Font *)fontForCharacter,
                                        rcText.bottom - 2, character, len,
                                        self->fore, self->back);
        } else {
            Surface_FillRectangle(surface, rcWhole, self->back);
        }
        break;
    }
}
