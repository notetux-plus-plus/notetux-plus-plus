/* Scintilla source code edit control
 * LineMarker.h — C translation of scintilla/src/LineMarker.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   Scintilla::MarkerSymbol     →  MarkerSymbol plain C enum
 *   Scintilla::MarginType       →  MarginType plain C enum
 *   Scintilla::Layer            →  Layer plain C enum
 *   Scintilla::Alpha            →  Alpha plain C enum
 *   enum class FoldPart         →  FoldPart plain C enum
 *   std::unique_ptr<XPM>        →  XPM *pxpm  (heap-owned by LineMarker)
 *   std::unique_ptr<RGBAImage>  →  RGBAImage *image (heap-owned)
 *   copy constructor/assignment →  LineMarker_copy()
 *   DrawLineMarkerFn typedef    →  kept as function pointer typedef
 */

#ifndef LINEMARKER_C_H
#define LINEMARKER_C_H

#include "Geometry.h"
#include "Surface.h"
#include "XPM.h"

/* ── Scintilla public enums ───────────────────────────────────────────── */
typedef enum {
    MarkerSymbol_Circle             =  0,
    MarkerSymbol_RoundRect          =  1,
    MarkerSymbol_Arrow              =  2,
    MarkerSymbol_SmallRect          =  3,
    MarkerSymbol_ShortArrow         =  4,
    MarkerSymbol_Empty              =  5,
    MarkerSymbol_ArrowDown          =  6,
    MarkerSymbol_Minus              =  7,
    MarkerSymbol_Plus               =  8,
    MarkerSymbol_VLine              =  9,
    MarkerSymbol_LCorner            = 10,
    MarkerSymbol_TCorner            = 11,
    MarkerSymbol_BoxPlus            = 12,
    MarkerSymbol_BoxPlusConnected   = 13,
    MarkerSymbol_BoxMinus           = 14,
    MarkerSymbol_BoxMinusConnected  = 15,
    MarkerSymbol_LCornerCurve       = 16,
    MarkerSymbol_TCornerCurve       = 17,
    MarkerSymbol_CirclePlus         = 18,
    MarkerSymbol_CirclePlusConnected  = 19,
    MarkerSymbol_CircleMinus        = 20,
    MarkerSymbol_CircleMinusConnected = 21,
    MarkerSymbol_Background         = 22,
    MarkerSymbol_DotDotDot          = 23,
    MarkerSymbol_Arrows             = 24,
    MarkerSymbol_Pixmap             = 25,
    MarkerSymbol_FullRect           = 26,
    MarkerSymbol_LeftRect           = 27,
    MarkerSymbol_Available          = 28,
    MarkerSymbol_Underline          = 29,
    MarkerSymbol_RgbaImage          = 30,
    MarkerSymbol_Bookmark           = 31,
    MarkerSymbol_VerticalBookmark   = 32,
    MarkerSymbol_Bar                = 33,
    MarkerSymbol_Character          = 10000
} MarkerSymbol;

typedef enum {
    MarginType_Symbol  = 0,
    MarginType_Number  = 1,
    MarginType_Back    = 2,
    MarginType_Fore    = 3,
    MarginType_Text    = 4,
    MarginType_RText   = 5,
    MarginType_Colour  = 6
} MarginType;

typedef enum {
    Layer_Base      = 0,
    Layer_UnderText = 1,
    Layer_OverText  = 2
} Layer;

typedef enum {
    Alpha_Transparent = 0,
    Alpha_Opaque      = 255,
    Alpha_NoAlpha     = 256
} Alpha;

/* ── FoldPart ─────────────────────────────────────────────────────────── */
typedef enum {
    FoldPart_undefined,
    FoldPart_head,
    FoldPart_body,
    FoldPart_tail,
    FoldPart_headWithTail
} FoldPart;

/* ── DrawLineMarkerFn ─────────────────────────────────────────────────── */
typedef void (*DrawLineMarkerFn)(Surface *surface, PRectangle rcWhole,
                                  const Font *fontForCharacter, int tFold,
                                  MarginType marginStyle, const void *lineMarker);

/* ── LineMarker ───────────────────────────────────────────────────────── */
typedef struct {
    MarkerSymbol       markType;
    ColourRGBA         fore;
    ColourRGBA         back;
    ColourRGBA         backSelected;
    Layer              layer;
    Alpha              alpha;
    XYPOSITION         strokeWidth;
    XPM               *pxpm;       /* heap-owned; NULL if not set */
    RGBAImage         *image;      /* heap-owned; NULL if not set */
    DrawLineMarkerFn   customDraw; /* NULL = use built-in Draw */
} LineMarker;

void LineMarker_init   (LineMarker *self);
void LineMarker_destroy(LineMarker *self);
void LineMarker_copy   (LineMarker *dst, const LineMarker *src);

ColourRGBA LineMarker_BackWithAlpha(const LineMarker *self);

void LineMarker_SetXPM       (LineMarker *self, const char *textForm);
void LineMarker_SetXPMLines  (LineMarker *self, const char *const *linesForm);
void LineMarker_SetRGBAImage (LineMarker *self, Point sizeRGBAImage, float scale,
                               const unsigned char *pixelsRGBAImage);

void LineMarker_Draw         (const LineMarker *self, Surface *surface,
                               PRectangle rcWhole, const Font *fontForCharacter,
                               FoldPart part, MarginType marginStyle);
void LineMarker_DrawFoldingMark(const LineMarker *self, Surface *surface,
                                PRectangle rcWhole, FoldPart part);

#endif /* LINEMARKER_C_H */
