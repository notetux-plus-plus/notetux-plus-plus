/* Scintilla source code edit control
 * Surface.h — C vtable declaration for the abstract Surface drawing interface.
 *
 * In C++, Surface is a pure-virtual class in scintilla/src/Platform.h.
 * Here it becomes a C vtable struct.  The concrete implementation lives in
 * PlatGTK.c (Phase 8); this header is the forward declaration that all
 * Phase 3–7 translated files can include safely.
 *
 * ── Translation notes ─────────────────────────────────────────────────
 *
 *   virtual methods                 →  function pointers in Surface_vtbl
 *   Surface::GradientOptions        →  GradientOptions plain C enum
 *   Surface::Ends                   →  SurfaceEnds plain C enum
 *   std::vector<ColourStop> stops   →  const ColourStop *stops, int nstops
 *   std::string_view text           →  const char *text, size_t len
 *   std::unique_ptr<Surface>        →  Surface * (heap-owned by caller)
 *   Font *, IScreenLine * etc.      →  opaque forward declarations
 */

#ifndef SURFACE_C_H
#define SURFACE_C_H

#include <stddef.h>
#include "Geometry.h"

/* ── Opaque forward declarations for types not yet translated ─────────── */
typedef struct Font              Font;
typedef struct IScreenLine       IScreenLine;
typedef struct IScreenLineLayout IScreenLineLayout;
typedef struct SurfaceMode       SurfaceMode;

/* ── Enums ────────────────────────────────────────────────────────────── */
typedef enum {
    GRADIENT_LEFT_TO_RIGHT,
    GRADIENT_TOP_TO_BOTTOM
} GradientOptions;

typedef enum {
    SURFACE_ENDS_SEMI_CIRCLES = 0x00,
    SURFACE_ENDS_LEFT_FLAT    = 0x01,
    SURFACE_ENDS_LEFT_ANGLE   = 0x02,
    SURFACE_ENDS_RIGHT_FLAT   = 0x10,
    SURFACE_ENDS_RIGHT_ANGLE  = 0x20
} SurfaceEnds;

/* ── Surface vtable ───────────────────────────────────────────────────── */
typedef struct Surface Surface;

typedef struct {
    /* Lifecycle */
    void      (*Release)            (Surface *self);
    int       (*Initialised)        (Surface *self);

    /* Configuration */
    void      (*SetMode)            (Surface *self, SurfaceMode mode);
    int       (*SupportsFeature)    (Surface *self, int feature);
    int       (*LogPixelsY)         (Surface *self);
    int       (*PixelDivisions)     (Surface *self);
    int       (*DeviceHeightFont)   (Surface *self, int points);

    /* Primitive drawing */
    void      (*LineDraw)           (Surface *self, Point start, Point end, Stroke stroke);
    void      (*PolyLine)           (Surface *self, const Point *pts, size_t npts, Stroke stroke);
    void      (*Polygon)            (Surface *self, const Point *pts, size_t npts, FillStroke fs);
    void      (*RectangleDraw)      (Surface *self, PRectangle rc, FillStroke fs);
    void      (*RectangleFrame)     (Surface *self, PRectangle rc, Stroke stroke);
    void      (*FillRectangle)      (Surface *self, PRectangle rc, ColourRGBA colour);
    void      (*FillRectangleAligned)(Surface *self, PRectangle rc, ColourRGBA colour);
    void      (*FillRectanglePattern)(Surface *self, PRectangle rc, Surface *pattern);
    void      (*RoundedRectangle)   (Surface *self, PRectangle rc, FillStroke fs);
    void      (*AlphaRectangle)     (Surface *self, PRectangle rc, XYPOSITION cornerSize, FillStroke fs);
    void      (*GradientRectangle)  (Surface *self, PRectangle rc,
                                     const ColourStop *stops, int nstops,
                                     GradientOptions options);
    void      (*DrawRGBAImage)      (Surface *self, PRectangle rc,
                                     int width, int height,
                                     const unsigned char *pixelsImage);
    void      (*Ellipse)            (Surface *self, PRectangle rc, FillStroke fs);
    void      (*Stadium)            (Surface *self, PRectangle rc, FillStroke fs, SurfaceEnds ends);
    void      (*Copy)               (Surface *self, PRectangle rc, Point from, Surface *source);

    /* Text drawing */
    void      (*DrawTextNoClip)     (Surface *self, PRectangle rc, Font *font,
                                     XYPOSITION ybase, const char *text, size_t len,
                                     ColourRGBA fore, ColourRGBA back);
    void      (*DrawTextClipped)    (Surface *self, PRectangle rc, Font *font,
                                     XYPOSITION ybase, const char *text, size_t len,
                                     ColourRGBA fore, ColourRGBA back);
    void      (*DrawTextTransparent)(Surface *self, PRectangle rc, Font *font,
                                     XYPOSITION ybase, const char *text, size_t len,
                                     ColourRGBA fore);
    void      (*MeasureWidths)      (Surface *self, Font *font,
                                     const char *text, size_t len, XYPOSITION *positions);
    XYPOSITION(*WidthText)          (Surface *self, Font *font,
                                     const char *text, size_t len);
    /* UTF-8 text variants */
    void      (*DrawTextNoClipUTF8)     (Surface *self, PRectangle rc, Font *font,
                                         XYPOSITION ybase, const char *text, size_t len,
                                         ColourRGBA fore, ColourRGBA back);
    void      (*DrawTextClippedUTF8)    (Surface *self, PRectangle rc, Font *font,
                                         XYPOSITION ybase, const char *text, size_t len,
                                         ColourRGBA fore, ColourRGBA back);
    void      (*DrawTextTransparentUTF8)(Surface *self, PRectangle rc, Font *font,
                                         XYPOSITION ybase, const char *text, size_t len,
                                         ColourRGBA fore);
    void      (*MeasureWidthsUTF8)      (Surface *self, Font *font,
                                         const char *text, size_t len, XYPOSITION *positions);
    XYPOSITION(*WidthTextUTF8)          (Surface *self, Font *font,
                                         const char *text, size_t len);

    /* Font metrics */
    XYPOSITION(*Ascent)             (Surface *self, Font *font);
    XYPOSITION(*Descent)            (Surface *self, Font *font);
    XYPOSITION(*InternalLeading)    (Surface *self, Font *font);
    XYPOSITION(*Height)             (Surface *self, Font *font);
    XYPOSITION(*AverageCharWidth)   (Surface *self, Font *font);

    /* Clip */
    void      (*SetClip)            (Surface *self, PRectangle rc);
    void      (*PopClip)            (Surface *self);
    void      (*FlushCachedState)   (Surface *self);
    void      (*FlushDrawing)       (Surface *self);
} Surface_vtbl;

struct Surface { const Surface_vtbl *vtbl; /* must be first */ };

/* ── Inline dispatch helpers ──────────────────────────────────────────── */
static inline void       Surface_Release        (Surface *s)                    { s->vtbl->Release(s); }
static inline int        Surface_Initialised    (Surface *s)                    { return s->vtbl->Initialised(s); }
static inline int        Surface_PixelDivisions (Surface *s)                    { return s->vtbl->PixelDivisions(s); }
static inline int        Surface_LogPixelsY     (Surface *s)                    { return s->vtbl->LogPixelsY(s); }
static inline int        Surface_DeviceHeightFont(Surface *s, int pt)           { return s->vtbl->DeviceHeightFont(s, pt); }
static inline void       Surface_LineDraw       (Surface *s, Point a, Point b, Stroke k)  { s->vtbl->LineDraw(s,a,b,k); }
static inline void       Surface_PolyLine       (Surface *s, const Point *pts, size_t n, Stroke k) { s->vtbl->PolyLine(s,pts,n,k); }
static inline void       Surface_Polygon        (Surface *s, const Point *pts, size_t n, FillStroke fs) { s->vtbl->Polygon(s,pts,n,fs); }
static inline void       Surface_RectangleDraw  (Surface *s, PRectangle rc, FillStroke fs) { s->vtbl->RectangleDraw(s,rc,fs); }
static inline void       Surface_RectangleFrame (Surface *s, PRectangle rc, Stroke k)      { s->vtbl->RectangleFrame(s,rc,k); }
static inline void       Surface_FillRectangle  (Surface *s, PRectangle rc, ColourRGBA c)  { s->vtbl->FillRectangle(s,rc,c); }
static inline void       Surface_FillRectangleAligned(Surface *s, PRectangle rc, ColourRGBA c) { s->vtbl->FillRectangleAligned(s,rc,c); }
static inline void       Surface_AlphaRectangle (Surface *s, PRectangle rc, XYPOSITION corner, FillStroke fs) { s->vtbl->AlphaRectangle(s,rc,corner,fs); }
static inline void       Surface_GradientRectangle(Surface *s, PRectangle rc, const ColourStop *stops, int n, GradientOptions opts) { s->vtbl->GradientRectangle(s,rc,stops,n,opts); }
static inline void       Surface_DrawRGBAImage  (Surface *s, PRectangle rc, int w, int h, const unsigned char *px) { s->vtbl->DrawRGBAImage(s,rc,w,h,px); }
static inline void       Surface_Ellipse        (Surface *s, PRectangle rc, FillStroke fs) { s->vtbl->Ellipse(s,rc,fs); }
static inline void       Surface_RoundedRectangle(Surface *s, PRectangle rc, FillStroke fs) { s->vtbl->RoundedRectangle(s,rc,fs); }
static inline void       Surface_SetClip        (Surface *s, PRectangle rc)     { s->vtbl->SetClip(s,rc); }
static inline void       Surface_PopClip        (Surface *s)                    { s->vtbl->PopClip(s); }
static inline void       Surface_FlushCachedState(Surface *s)                   { s->vtbl->FlushCachedState(s); }
static inline void       Surface_FlushDrawing   (Surface *s)                    { s->vtbl->FlushDrawing(s); }
static inline XYPOSITION Surface_WidthText      (Surface *s, Font *f, const char *t, size_t l) { return s->vtbl->WidthText(s,f,t,l); }
static inline XYPOSITION Surface_WidthTextUTF8  (Surface *s, Font *f, const char *t, size_t l) { return s->vtbl->WidthTextUTF8(s,f,t,l); }
static inline XYPOSITION Surface_Ascent         (Surface *s, Font *f)           { return s->vtbl->Ascent(s,f); }
static inline XYPOSITION Surface_Descent        (Surface *s, Font *f)           { return s->vtbl->Descent(s,f); }
static inline XYPOSITION Surface_InternalLeading(Surface *s, Font *f)           { return s->vtbl->InternalLeading(s,f); }
static inline XYPOSITION Surface_Height         (Surface *s, Font *f)           { return s->vtbl->Height(s,f); }
static inline XYPOSITION Surface_AverageCharWidth(Surface *s, Font *f)          { return s->vtbl->AverageCharWidth(s,f); }

#endif /* SURFACE_C_H */
