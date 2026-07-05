/* Scintilla source code edit control
 * Style.c — C translation of scintilla/src/Style.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   FontSpecification::operator==   →  FontSpec_eq
 *   FontSpecification::operator<    →  FontSpec_lt
 *   Platform::DefaultFontSize()     →  Platform_DefaultFontSize() (PlatGTK.c)
 *   Style::Style(fontName)          →  Style_init
 *   Style::Copy(shared_ptr, fm)     →  Style_Copy  (Font * is non-owning)
 *   static_cast<FontMeasurements&>(*this) = fm_  →  memcpy into self->fontMeas
 *   std::shared_ptr<Font>           →  Font *  (raw, ViewStyle owns lifetime)
 */

#include <string.h>
#include "Style.h"

/* ── FontSpecification comparators ──���────────────────────────────────── */

int FontSpec_eq(const FontSpecification *a, const FontSpecification *b) {
    return a->fontName       == b->fontName        /* pointer equality — interned */
        && a->weight         == b->weight
        && a->italic         == b->italic
        && a->size           == b->size
        && a->stretch        == b->stretch
        && a->characterSet   == b->characterSet
        && a->extraFontFlag  == b->extraFontFlag
        && a->checkMonospaced== b->checkMonospaced;
}

int FontSpec_lt(const FontSpecification *a, const FontSpecification *b) {
    if (a->fontName      != b->fontName)      return a->fontName < b->fontName;
    if (a->weight        != b->weight)        return a->weight   < b->weight;
    if (a->italic        != b->italic)        return !a->italic;
    if (a->size          != b->size)          return a->size     < b->size;
    if (a->stretch       != b->stretch)       return a->stretch  < b->stretch;
    if (a->characterSet  != b->characterSet)  return a->characterSet  < b->characterSet;
    if (a->extraFontFlag != b->extraFontFlag) return a->extraFontFlag < b->extraFontFlag;
    if (a->checkMonospaced != b->checkMonospaced) return !a->checkMonospaced;
    return 0;
}

/* ── Style ────────────────────────────────────────────────────────────── */

void Style_init(Style *self, const char *fontName) {
    self->fontSpec        = FontSpec_make(fontName,
                                          Platform_DefaultFontSize() * FONT_SIZE_MULTIPLIER);
    self->fontMeas        = FontMeas_default();
    self->fore            = ColourRGBA_black;
    self->back            = ColourRGBA_white;
    self->eolFilled       = 0;
    self->underline       = 0;
    self->caseForce       = CaseForce_mixed;
    self->visible         = 1;
    self->changeable      = 1;
    self->hotspot         = 0;
    memset(self->invisibleRepresentation, 0, sizeof(self->invisibleRepresentation));
    self->font            = NULL;
}

void Style_Copy(Style *self, Font *font, const FontMeasurements *fm) {
    self->font     = font;
    self->fontMeas = *fm;
}
