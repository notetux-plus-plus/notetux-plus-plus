/* Scintilla source code edit control
 * Geometry.c — C translation of scintilla/src/Geometry.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   All constexpr/inline methods → static inline in Geometry.h
 *   Non-trivial free functions    → implemented here
 *
 *   std::clamp / std::min / std::max  →  GEO_CLAMP / GEO_MIN / GEO_MAX
 *   std::round / std::floor / std::ceil  →  round / floor / ceil  (<math.h>)
 *   enum class Edge { left,top,bottom,right }  →  EDGE_LEFT/TOP/BOTTOM/RIGHT
 *   Intersection() overloads  →  Intersection_iv / Intersection_rc
 *   PixelAlign() overloads    →  PixelAlign / PixelAlign_pt / PixelAlign_rc
 *   ColourRGBA::MixedWith()   →  ColourRGBA_MixedWith / _MixedWithProportion
 *
 *   anonymous-namespace Mixed() helper → static Mixed() local to this file
 */

#include <math.h>
#include "Geometry.h"

/* ── File-private helpers ────────────────────────────────────────────── */

static unsigned int Mixed(unsigned char a, unsigned char b, double proportion) {
    return (unsigned int)(a + proportion * (b - a));
}

/* ── Clamp / Side ────────────────────────────────────────────────────── */

PRectangle Clamp(PRectangle rc, Edge edge, XYPOSITION position) {
    switch (edge) {
    case EDGE_LEFT:
        return PRectangle_make(GEO_CLAMP(position, rc.left, rc.right),
                               rc.top, rc.right, rc.bottom);
    case EDGE_TOP:
        return PRectangle_make(rc.left,
                               GEO_CLAMP(position, rc.top, rc.bottom),
                               rc.right, rc.bottom);
    case EDGE_RIGHT:
        return PRectangle_make(rc.left, rc.top,
                               GEO_CLAMP(position, rc.left, rc.right),
                               rc.bottom);
    case EDGE_BOTTOM:
    default:
        return PRectangle_make(rc.left, rc.top, rc.right,
                               GEO_CLAMP(position, rc.top, rc.bottom));
    }
}

PRectangle Side(PRectangle rc, Edge edge, XYPOSITION size) {
    switch (edge) {
    case EDGE_LEFT:
        return PRectangle_make(rc.left, rc.top,
                               GEO_MIN(rc.left + size, rc.right), rc.bottom);
    case EDGE_TOP:
        return PRectangle_make(rc.left, rc.top,
                               rc.right, GEO_MIN(rc.top + size, rc.bottom));
    case EDGE_RIGHT:
        return PRectangle_make(GEO_MAX(rc.left, rc.right - size),
                               rc.top, rc.right, rc.bottom);
    case EDGE_BOTTOM:
    default:
        return PRectangle_make(rc.left,
                               GEO_MAX(rc.top, rc.bottom - size),
                               rc.right, rc.bottom);
    }
}

/* ── Intersection / HorizontalBounds ────────────────────────────────── */

Interval Intersection_iv(Interval a, Interval b) {
    const XYPOSITION leftMax  = GEO_MAX(a.left,  b.left);
    const XYPOSITION rightMin = GEO_MIN(a.right, b.right);
    const XYPOSITION rightResult = (rightMin >= leftMax) ? rightMin : leftMax;
    Interval r; r.left = leftMax; r.right = rightResult; return r;
}

PRectangle Intersection_rc(PRectangle rc, Interval hb) {
    const Interval iv = Intersection_iv(HorizontalBounds(rc), hb);
    return PRectangle_make(iv.left, rc.top, iv.right, rc.bottom);
}

Interval HorizontalBounds(PRectangle rc) {
    Interval r; r.left = rc.left; r.right = rc.right; return r;
}

/* ── PixelAlign ──────────────────────────────────────────────────────── */

XYPOSITION PixelAlign(XYPOSITION xy, int pixelDivisions) {
    return round(xy * pixelDivisions) / pixelDivisions;
}

XYPOSITION PixelAlignFloor(XYPOSITION xy, int pixelDivisions) {
    return floor(xy * pixelDivisions) / pixelDivisions;
}

XYPOSITION PixelAlignCeil(XYPOSITION xy, int pixelDivisions) {
    return ceil(xy * pixelDivisions) / pixelDivisions;
}

Point PixelAlign_pt(Point pt, int pixelDivisions) {
    return Point_make(PixelAlign(pt.x, pixelDivisions),
                      PixelAlign(pt.y, pixelDivisions));
}

PRectangle PixelAlign_rc(PRectangle rc, int pixelDivisions) {
    return PRectangle_make(
        PixelAlign     (rc.left,   pixelDivisions),
        PixelAlignFloor(rc.top,    pixelDivisions),
        PixelAlign     (rc.right,  pixelDivisions),
        PixelAlignFloor(rc.bottom, pixelDivisions));
}

PRectangle PixelAlignOutside(PRectangle rc, int pixelDivisions) {
    return PRectangle_make(
        PixelAlignFloor(rc.left,   pixelDivisions),
        PixelAlignFloor(rc.top,    pixelDivisions),
        PixelAlignCeil (rc.right,  pixelDivisions),
        PixelAlignFloor(rc.bottom, pixelDivisions));
}

/* ── ColourRGBA mixing ───────────────────────────────────────────────── */

ColourRGBA ColourRGBA_MixedWith(ColourRGBA self, ColourRGBA other) {
    const unsigned int red   = (ColourRGBA_GetRed(self)   + ColourRGBA_GetRed(other))   / 2;
    const unsigned int green = (ColourRGBA_GetGreen(self) + ColourRGBA_GetGreen(other)) / 2;
    const unsigned int blue  = (ColourRGBA_GetBlue(self)  + ColourRGBA_GetBlue(other))  / 2;
    const unsigned int alpha = (ColourRGBA_GetAlpha(self) + ColourRGBA_GetAlpha(other)) / 2;
    return ColourRGBA_make(red, green, blue, alpha);
}

ColourRGBA ColourRGBA_MixedWithProportion(ColourRGBA self, ColourRGBA other,
                                           double proportion) {
    return ColourRGBA_make(
        Mixed(ColourRGBA_GetRed(self),   ColourRGBA_GetRed(other),   proportion),
        Mixed(ColourRGBA_GetGreen(self), ColourRGBA_GetGreen(other), proportion),
        Mixed(ColourRGBA_GetBlue(self),  ColourRGBA_GetBlue(other),  proportion),
        Mixed(ColourRGBA_GetAlpha(self), ColourRGBA_GetAlpha(other), proportion));
}
