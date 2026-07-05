/* Scintilla source code edit control
 * Indicator.h — C translation of scintilla/src/Indicator.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   Scintilla::IndicatorStyle  →  IndicatorStyle plain C enum
 *   Scintilla::IndicFlag       →  IndicFlag plain C enum
 *   Scintilla::IndicValue      →  IndicValue_Mask / IndicValue_Bit constants
 *   struct StyleAndColour      →  StyleAndColour (plain C struct)
 *   class Indicator            →  Indicator struct + Indicator_* functions
 *   enum class State           →  IndicatorState plain C enum
 */

#ifndef INDICATOR_C_H
#define INDICATOR_C_H

#include "Geometry.h"
#include "Surface.h"

/* ── Scintilla public enums (from ScintillaTypes.h) ───────────────────── */
typedef enum {
    IndicatorStyle_Plain          =  0,
    IndicatorStyle_Squiggle       =  1,
    IndicatorStyle_TT             =  2,
    IndicatorStyle_Diagonal       =  3,
    IndicatorStyle_Strike         =  4,
    IndicatorStyle_Hidden         =  5,
    IndicatorStyle_Box            =  6,
    IndicatorStyle_RoundBox       =  7,
    IndicatorStyle_StraightBox    =  8,
    IndicatorStyle_Dash           =  9,
    IndicatorStyle_Dots           = 10,
    IndicatorStyle_SquiggleLow    = 11,
    IndicatorStyle_DotBox         = 12,
    IndicatorStyle_SquigglePixmap = 13,
    IndicatorStyle_CompositionThick = 14,
    IndicatorStyle_CompositionThin  = 15,
    IndicatorStyle_FullBox        = 16,
    IndicatorStyle_TextFore       = 17,
    IndicatorStyle_Point          = 18,
    IndicatorStyle_PointCharacter = 19,
    IndicatorStyle_Gradient       = 20,
    IndicatorStyle_GradientCentre = 21,
    IndicatorStyle_PointTop       = 22,
    IndicatorStyle_ExplorerLink   = 23
} IndicatorStyle;

typedef enum {
    IndicFlag_None      = 0,
    IndicFlag_ValueFore = 1
} IndicFlag;

enum {
    IndicValue_Bit  = 0x1000000,
    IndicValue_Mask = 0xFFFFFF
};

/* ── StyleAndColour ──────────────────────────────────────────────────── */
typedef struct {
    IndicatorStyle style;
    ColourRGBA     fore;
} StyleAndColour;

static inline StyleAndColour StyleAndColour_make(IndicatorStyle style, ColourRGBA fore) {
    StyleAndColour sac; sac.style = style; sac.fore = fore; return sac;
}
static inline int StyleAndColour_eq(StyleAndColour a, StyleAndColour b) {
    return a.style == b.style && ColourRGBA_eq(a.fore, b.fore);
}

/* ── IndicatorState ──────────────────────────────────────────────────── */
typedef enum { IndicatorState_Normal, IndicatorState_Hover } IndicatorState;

/* ── Indicator ───────────────────────────────────────────────────────── */
typedef struct {
    StyleAndColour sacNormal;
    StyleAndColour sacHover;
    int            under;
    int            fillAlpha;
    int            outlineAlpha;
    IndicFlag      attributes;
    XYPOSITION     strokeWidth;
} Indicator;

void Indicator_init    (Indicator *self);
void Indicator_initFull(Indicator *self, IndicatorStyle style, ColourRGBA fore,
                        int under, int fillAlpha, int outlineAlpha);

void Indicator_Draw    (const Indicator *self, Surface *surface,
                        PRectangle rc, PRectangle rcLine, PRectangle rcCharacter,
                        IndicatorState state, int value);

static inline int  Indicator_IsDynamic(const Indicator *self) {
    return !StyleAndColour_eq(self->sacNormal, self->sacHover);
}
static inline int  Indicator_OverridesTextFore(const Indicator *self) {
    return self->sacNormal.style == IndicatorStyle_TextFore
        || self->sacHover.style  == IndicatorStyle_TextFore
        || self->sacNormal.style == IndicatorStyle_ExplorerLink
        || self->sacHover.style  == IndicatorStyle_ExplorerLink;
}
static inline IndicFlag Indicator_Flags(const Indicator *self) {
    return self->attributes;
}
void Indicator_SetFlags(Indicator *self, IndicFlag attributes);

#endif /* INDICATOR_C_H */
