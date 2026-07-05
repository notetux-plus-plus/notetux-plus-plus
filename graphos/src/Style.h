/* Scintilla source code edit control
 * Style.h — C translation of scintilla/src/Style.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   Scintilla enums (FontWeight, FontStretch, CharacterSet, FontQuality)
 *     →  plain C enums with their canonical integer values
 *   FontSizeMultiplier          →  FONT_SIZE_MULTIPLIER macro
 *   struct FontSpecification    →  FontSpecification plain C struct
 *   operator== / operator<      →  FontSpec_eq / FontSpec_lt
 *   struct FontMeasurements     →  FontMeasurements plain C struct
 *   class Style : public FontSpecification, public FontMeasurements
 *     →  Style struct embedding both by value (fontSpec, fontMeas)
 *   enum class CaseForce        →  CaseForce plain C enum
 *   std::shared_ptr<Font> font  →  Font *font  (non-owning; ViewStyle owns it)
 *   Style::Copy(shared_ptr,fm)  →  Style_Copy(Style *, Font *, FontMeasurements *)
 *   Platform::DefaultFontSize() →  Platform_DefaultFontSize() — declared here,
 *                                   implemented in PlatGTK.c (Phase 8)
 */

#ifndef STYLE_C_H
#define STYLE_C_H

#include <string.h>
#include "Geometry.h"
#include "Surface.h"   /* for Font forward declaration */

/* ── Scintilla font/character enums ──────────────────────────────────── */

#define FONT_SIZE_MULTIPLIER 100

typedef enum {
    FontWeight_Normal   = 400,
    FontWeight_SemiBold = 600,
    FontWeight_Bold     = 700
} FontWeight;

typedef enum {
    FontStretch_UltraCondensed = 1,
    FontStretch_ExtraCondensed = 2,
    FontStretch_Condensed      = 3,
    FontStretch_SemiCondensed  = 4,
    FontStretch_Normal         = 5,
    FontStretch_SemiExpanded   = 6,
    FontStretch_Expanded       = 7,
    FontStretch_ExtraExpanded  = 8,
    FontStretch_UltraExpanded  = 9
} FontStretch;

typedef enum {
    CharacterSet_Ansi       =    0,
    CharacterSet_Default    =    1,
    CharacterSet_Baltic     =  186,
    CharacterSet_ChineseBig5=  136,
    CharacterSet_EastEurope =  238,
    CharacterSet_GB2312     =  134,
    CharacterSet_Greek      =  161,
    CharacterSet_Hangul     =  129,
    CharacterSet_Mac        =   77,
    CharacterSet_Oem        =  255,
    CharacterSet_Russian    =  204,
    CharacterSet_Oem866     =  866,
    CharacterSet_Cyrillic   = 1251,
    CharacterSet_ShiftJis   =  128,
    CharacterSet_Symbol     =    2,
    CharacterSet_Turkish    =  162,
    CharacterSet_Johab      =  130,
    CharacterSet_Hebrew     =  177,
    CharacterSet_Arabic     =  178,
    CharacterSet_Vietnamese =  163,
    CharacterSet_Thai       =  222,
    CharacterSet_Iso8859_15 = 1000
} CharacterSet;

typedef enum {
    FontQuality_QualityMask           = 0xF,
    FontQuality_QualityDefault        = 0,
    FontQuality_QualityNonAntialiased = 1,
    FontQuality_QualityAntialiased    = 2,
    FontQuality_QualityLcdOptimized   = 3
} FontQuality;

/* ── Platform_DefaultFontSize — implemented in PlatGTK.c ────────────── */
int Platform_DefaultFontSize(void);

/* ── FontSpecification ───────────────────────────────────────────────── */
typedef struct {
    const char   *fontName;      /* interned by UniqueStringSet in ViewStyle */
    int           size;          /* in units of FONT_SIZE_MULTIPLIER (i.e. points×100) */
    FontWeight    weight;
    FontStretch   stretch;
    int           italic;
    CharacterSet  characterSet;
    FontQuality   extraFontFlag;
    int           checkMonospaced;
} FontSpecification;

static inline FontSpecification FontSpec_make(const char *name, int size) {
    FontSpecification fs;
    fs.fontName        = name;
    fs.size            = size;
    fs.weight          = FontWeight_Normal;
    fs.stretch         = FontStretch_Normal;
    fs.italic          = 0;
    fs.characterSet    = CharacterSet_Default;
    fs.extraFontFlag   = FontQuality_QualityDefault;
    fs.checkMonospaced = 0;
    return fs;
}

int FontSpec_eq(const FontSpecification *a, const FontSpecification *b);
int FontSpec_lt(const FontSpecification *a, const FontSpecification *b);

/* ── FontMeasurements ────────────────────────────────────────────────── */
typedef struct {
    XYPOSITION ascent;
    XYPOSITION descent;
    XYPOSITION capitalHeight;          /* ascent - internal leading */
    XYPOSITION aveCharWidth;
    XYPOSITION monospaceCharacterWidth;
    XYPOSITION spaceWidth;
    int        monospaceASCII;
    int        sizeZoomed;
} FontMeasurements;

static inline FontMeasurements FontMeas_default(void) {
    FontMeasurements fm;
    fm.ascent                 = 1;
    fm.descent                = 1;
    fm.capitalHeight          = 1;
    fm.aveCharWidth           = 1;
    fm.monospaceCharacterWidth= 1;
    fm.spaceWidth             = 1;
    fm.monospaceASCII         = 0;
    fm.sizeZoomed             = 2;
    return fm;
}

/* ── CaseForce ───────────────────────────────────────────────────────── */
typedef enum { CaseForce_mixed, CaseForce_upper, CaseForce_lower, CaseForce_camel } CaseForce;

/* ── Style ───────────────────────────────────────────────────────────── */
typedef struct {
    FontSpecification fontSpec;   /* must be first — cast safely to FontSpecification * */
    FontMeasurements  fontMeas;
    ColourRGBA        fore;
    ColourRGBA        back;
    int               eolFilled;
    int               underline;
    CaseForce         caseForce;
    int               visible;
    int               changeable;
    int               hotspot;
    char              invisibleRepresentation[5];
    Font             *font;       /* non-owning pointer; managed by ViewStyle */
} Style;

void Style_init  (Style *self, const char *fontName);
void Style_Copy  (Style *self, Font *font, const FontMeasurements *fm);

static inline int Style_IsProtected(const Style *self) {
    return !(self->changeable && self->visible);
}

#endif /* STYLE_C_H */
