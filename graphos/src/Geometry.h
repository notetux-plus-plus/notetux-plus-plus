/* Scintilla source code edit control
 * Geometry.h — C translation of scintilla/src/Geometry.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   class Point / Interval / PRectangle  →  plain C structs
 *   constexpr constructors               →  _make() inline helpers
 *   operator== / != / + / -              →  _eq / _ne / _add / _sub inlines
 *   constexpr methods                    →  static inline functions
 *   enum class Edge                      →  plain enum with EDGE_ prefix
 *   class ColourRGBA                     →  ColourRGBA struct (int co)
 *   ColourRGBA methods                   →  ColourRGBA_* inline functions
 *   constexpr ColourRGBA white/black     →  static const ColourRGBA
 *   class Stroke / Fill / FillStroke / ColourStop  →  plain C structs
 *   std::clamp / std::min / std::max     →  GEO_CLAMP / GEO_MIN / GEO_MAX macros
 *   MixedWith (non-trivial, uses double) →  implemented in Geometry.c
 */

#ifndef GEOMETRY_C_H
#define GEOMETRY_C_H

#include <stdint.h>
#include <math.h>

typedef double XYPOSITION;
typedef double XYACCUMULATOR;

/* ── Arithmetic helpers ──────────────────────────────────────────────── */
#define GEO_MIN(a,b)        ((a) < (b) ? (a) : (b))
#define GEO_MAX(a,b)        ((a) > (b) ? (a) : (b))
#define GEO_CLAMP(v,lo,hi)  ((v) < (lo) ? (lo) : (v) > (hi) ? (hi) : (v))

/* ── Point ───────────────────────────────────────────────────────────── */
typedef struct { XYPOSITION x; XYPOSITION y; } Point;

static inline Point Point_make(XYPOSITION x, XYPOSITION y) {
    Point p; p.x = x; p.y = y; return p;
}
static inline Point Point_FromInts(int x, int y) {
    return Point_make((XYPOSITION)x, (XYPOSITION)y);
}
static inline int   Point_eq (Point a, Point b) { return a.x==b.x && a.y==b.y; }
static inline int   Point_ne (Point a, Point b) { return a.x!=b.x || a.y!=b.y; }
static inline Point Point_add(Point a, Point b) { return Point_make(a.x+b.x, a.y+b.y); }
static inline Point Point_sub(Point a, Point b) { return Point_make(a.x-b.x, a.y-b.y); }

/* ── Interval ────────────────────────────────────────────────────────── */
typedef struct { XYPOSITION left; XYPOSITION right; } Interval;

static inline int        Interval_eq        (Interval a, Interval b) { return a.left==b.left && a.right==b.right; }
static inline XYPOSITION Interval_Width     (Interval iv)            { return iv.right - iv.left; }
static inline int        Interval_Empty     (Interval iv)            { return Interval_Width(iv) <= 0; }
static inline int        Interval_Intersects(Interval iv, Interval o){ return iv.right > o.left && iv.left < o.right; }
static inline Interval   Interval_Offset    (Interval iv, XYPOSITION off) {
    Interval r; r.left = iv.left+off; r.right = iv.right+off; return r;
}
static inline Interval   Interval_FromLeftAndWidth(XYPOSITION left, XYPOSITION width) {
    Interval r; r.left = left; r.right = left+width; return r;
}

/* ── PRectangle ──────────────────────────────────────────────────────── */
typedef struct {
    XYPOSITION left, top, right, bottom;
} PRectangle;

static inline PRectangle PRectangle_make(XYPOSITION l, XYPOSITION t,
                                          XYPOSITION r, XYPOSITION b) {
    PRectangle rc; rc.left=l; rc.top=t; rc.right=r; rc.bottom=b; return rc;
}
static inline PRectangle PRectangle_FromInts(int l, int t, int r, int b) {
    return PRectangle_make((XYPOSITION)l,(XYPOSITION)t,(XYPOSITION)r,(XYPOSITION)b);
}

static inline int PRectangle_eq(PRectangle a, PRectangle b) {
    return a.left==b.left && a.right==b.right && a.top==b.top && a.bottom==b.bottom;
}
static inline int PRectangle_ContainsPoint(PRectangle rc, Point pt) {
    return pt.x>=rc.left && pt.x<=rc.right && pt.y>=rc.top && pt.y<=rc.bottom;
}
static inline int PRectangle_ContainsWholePixel(PRectangle rc, Point pt) {
    return pt.x>=rc.left && (pt.x+1)<=rc.right && pt.y>=rc.top && (pt.y+1)<=rc.bottom;
}
static inline int PRectangle_ContainsRect(PRectangle rc, PRectangle o) {
    return o.left>=rc.left && o.right<=rc.right && o.top>=rc.top && o.bottom<=rc.bottom;
}
static inline int PRectangle_Intersects(PRectangle a, PRectangle b) {
    return a.right>b.left && a.left<b.right && a.bottom>b.top && a.top<b.bottom;
}
static inline int PRectangle_IntersectsInterval(PRectangle rc, Interval hb) {
    return rc.right > hb.left && rc.left < hb.right;
}
static inline void PRectangle_Move(PRectangle *rc, XYPOSITION dx, XYPOSITION dy) {
    rc->left+=dx; rc->top+=dy; rc->right+=dx; rc->bottom+=dy;
}
static inline PRectangle PRectangle_WithHorizontalBounds(PRectangle rc, Interval h) {
    return PRectangle_make(h.left, rc.top, h.right, rc.bottom);
}
static inline PRectangle PRectangle_InsetScalar(PRectangle rc, XYPOSITION d) {
    return PRectangle_make(rc.left+d, rc.top+d, rc.right-d, rc.bottom-d);
}
static inline PRectangle PRectangle_InsetPoint(PRectangle rc, Point d) {
    return PRectangle_make(rc.left+d.x, rc.top+d.y, rc.right-d.x, rc.bottom-d.y);
}
static inline Point PRectangle_Centre(PRectangle rc) {
    return Point_make((rc.left+rc.right)/2, (rc.top+rc.bottom)/2);
}
static inline XYPOSITION PRectangle_Width (PRectangle rc) { return rc.right  - rc.left; }
static inline XYPOSITION PRectangle_Height(PRectangle rc) { return rc.bottom - rc.top;  }
static inline int        PRectangle_Empty (PRectangle rc) {
    return PRectangle_Height(rc)<=0 || PRectangle_Width(rc)<=0;
}

/* ── Edge ────────────────────────────────────────────────────────────── */
typedef enum { EDGE_LEFT, EDGE_TOP, EDGE_BOTTOM, EDGE_RIGHT } Edge;

/* Declared in Geometry.c */
PRectangle Clamp              (PRectangle rc, Edge edge, XYPOSITION position);
PRectangle Side               (PRectangle rc, Edge edge, XYPOSITION size);
Interval   Intersection_iv    (Interval a, Interval b);
PRectangle Intersection_rc    (PRectangle rc, Interval horizontalBounds);
Interval   HorizontalBounds   (PRectangle rc);
XYPOSITION PixelAlign         (XYPOSITION xy, int pixelDivisions);
XYPOSITION PixelAlignFloor    (XYPOSITION xy, int pixelDivisions);
XYPOSITION PixelAlignCeil     (XYPOSITION xy, int pixelDivisions);
Point      PixelAlign_pt      (Point pt, int pixelDivisions);
PRectangle PixelAlign_rc      (PRectangle rc, int pixelDivisions);
PRectangle PixelAlignOutside  (PRectangle rc, int pixelDivisions);

/* ── ColourRGBA ──────────────────────────────────────────────────────── */
#define COLOUR_COMPONENT_MAX  255.0f
#define COLOUR_MAX_BYTE       0xFFu
#define COLOUR_RGB_MASK       0xFFFFFF

typedef struct { int co; } ColourRGBA;

static inline ColourRGBA ColourRGBA_make(unsigned int r, unsigned int g,
                                          unsigned int b, unsigned int a) {
    ColourRGBA c;
    c.co = (int)(r | (g<<8) | (b<<16) | (a<<24));
    return c;
}
static inline ColourRGBA ColourRGBA_fromInt(int co) {
    ColourRGBA c; c.co = co; return c;
}
static inline ColourRGBA ColourRGBA_FromRGB(int co) {
    ColourRGBA c; c.co = co | (int)(COLOUR_MAX_BYTE << 24); return c;
}
static inline ColourRGBA ColourRGBA_Grey(unsigned int grey, unsigned int alpha) {
    return ColourRGBA_make(grey, grey, grey, alpha);
}
static inline ColourRGBA ColourRGBA_FromIpRGB(intptr_t co) {
    return ColourRGBA_FromRGB((int)co & COLOUR_RGB_MASK);
}
static inline ColourRGBA ColourRGBA_WithAlpha(ColourRGBA cd, unsigned int alpha) {
    ColourRGBA c; c.co = (cd.co & COLOUR_RGB_MASK) | (int)(alpha << 24); return c;
}
static inline ColourRGBA ColourRGBA_WithoutAlpha(ColourRGBA c) {
    ColourRGBA r; r.co = c.co & COLOUR_RGB_MASK; return r;
}
static inline ColourRGBA ColourRGBA_Opaque(ColourRGBA c) {
    ColourRGBA r; r.co = c.co | (int)(COLOUR_MAX_BYTE << 24); return r;
}
static inline int          ColourRGBA_AsInteger  (ColourRGBA c) { return c.co; }
static inline int          ColourRGBA_OpaqueRGB  (ColourRGBA c) { return c.co & COLOUR_RGB_MASK; }
static inline unsigned char ColourRGBA_GetRed    (ColourRGBA c) { return (unsigned char)(c.co & COLOUR_MAX_BYTE); }
static inline unsigned char ColourRGBA_GetGreen  (ColourRGBA c) { return (unsigned char)((c.co >> 8)  & COLOUR_MAX_BYTE); }
static inline unsigned char ColourRGBA_GetBlue   (ColourRGBA c) { return (unsigned char)((c.co >> 16) & COLOUR_MAX_BYTE); }
static inline unsigned char ColourRGBA_GetAlpha  (ColourRGBA c) { return (unsigned char)((c.co >> 24) & COLOUR_MAX_BYTE); }
static inline float ColourRGBA_GetRedComponent  (ColourRGBA c) { return ColourRGBA_GetRed(c)   / COLOUR_COMPONENT_MAX; }
static inline float ColourRGBA_GetGreenComponent(ColourRGBA c) { return ColourRGBA_GetGreen(c) / COLOUR_COMPONENT_MAX; }
static inline float ColourRGBA_GetBlueComponent (ColourRGBA c) { return ColourRGBA_GetBlue(c)  / COLOUR_COMPONENT_MAX; }
static inline float ColourRGBA_GetAlphaComponent(ColourRGBA c) { return ColourRGBA_GetAlpha(c) / COLOUR_COMPONENT_MAX; }
static inline int   ColourRGBA_eq     (ColourRGBA a, ColourRGBA b) { return a.co == b.co; }
static inline int   ColourRGBA_IsOpaque(ColourRGBA c) { return ColourRGBA_GetAlpha(c) == COLOUR_MAX_BYTE; }

/* Declared in Geometry.c */
ColourRGBA ColourRGBA_MixedWith           (ColourRGBA self, ColourRGBA other);
ColourRGBA ColourRGBA_MixedWithProportion (ColourRGBA self, ColourRGBA other, double proportion);

static const ColourRGBA ColourRGBA_white = { -1 };          /* 0xFFFFFFFF — RGBA all 255 */
static const ColourRGBA ColourRGBA_black = { -16777216 };   /* 0xFF000000 — alpha=255, RGB=0 */

/* ── Stroke / Fill / FillStroke / ColourStop ─────────────────────────── */
typedef struct {
    ColourRGBA colour;
    XYPOSITION width;
} Stroke;

static inline Stroke Stroke_make(ColourRGBA colour, XYPOSITION width) {
    Stroke s; s.colour = colour; s.width = width; return s;
}
static inline float Stroke_WidthF(Stroke s) { return (float)s.width; }

typedef struct { ColourRGBA colour; } Fill;
static inline Fill Fill_make(ColourRGBA colour) { Fill f; f.colour = colour; return f; }

typedef struct { Fill fill; Stroke stroke; } FillStroke;
static inline FillStroke FillStroke_make2(ColourRGBA fill, ColourRGBA stroke, XYPOSITION w) {
    FillStroke fs;
    fs.fill   = Fill_make(fill);
    fs.stroke = Stroke_make(stroke, w);
    return fs;
}
static inline FillStroke FillStroke_make1(ColourRGBA both, XYPOSITION w) {
    return FillStroke_make2(both, both, w);
}

typedef struct { XYPOSITION position; ColourRGBA colour; } ColourStop;
static inline ColourStop ColourStop_make(XYPOSITION pos, ColourRGBA colour) {
    ColourStop cs; cs.position = pos; cs.colour = colour; return cs;
}

#endif /* GEOMETRY_C_H */
