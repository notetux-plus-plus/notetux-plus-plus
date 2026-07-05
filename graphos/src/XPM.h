/* Scintilla source code edit control
 * XPM.h — C translation of scintilla/src/XPM.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   class XPM
 *     std::vector<unsigned char> pixels     →  unsigned char *pixels
 *     ColourRGBA colourCodeTable[256]        →  kept as-is
 *     private helpers (ColourFromCode,
 *                       FillRun, LinesFromText)  →  static in XPM.c
 *
 *   class RGBAImage
 *     std::vector<unsigned char> pixelBytes  →  unsigned char *pixelBytes
 *     static constexpr bytesPerPixel = 4     →  RGBA_BYTES_PER_PIXEL macro
 *
 *   class RGBAImageSet
 *     std::map<int,unique_ptr<RGBAImage>>    →  sorted heap array of XPMEntry
 *     mutable int height/width (lazy cache)  →  int height/width (-1 = dirty)
 *
 *   Surface dependency (Phase 8 — PlatGTK.c not yet translated):
 *     Minimal Surface vtable defined here so XPM_Draw compiles.
 *     Platform.c will extend Surface_vtbl with additional members.
 */

#ifndef XPM_C_H
#define XPM_C_H

#include <stddef.h>
#include "Geometry.h"

/* ── Minimal Surface interface (extended by Platform.c, Phase 8) ─────── */
typedef struct Surface Surface;

typedef struct {
    void (*FillRectangle)(Surface *self, PRectangle rc, ColourRGBA colour);
    /* Platform.c adds more entries here */
} Surface_vtbl;

struct Surface { const Surface_vtbl *vtbl; /* must be first */ };

static inline void Surface_FillRectangle(Surface *self, PRectangle rc, ColourRGBA colour) {
    self->vtbl->FillRectangle(self, rc, colour);
}

/* ── XPM ─────────────────────────────────────────────────────────────── */
typedef struct {
    int             height;
    int             width;
    int             nColours;
    unsigned char  *pixels;          /* heap; width*height bytes */
    ColourRGBA      colourCodeTable[256];
    char            codeTransparent;
} XPM;

void       XPM_init_text  (XPM *self, const char *textForm);
void       XPM_init_lines (XPM *self, const char *const *linesForm);
void       XPM_destroy    (XPM *self);
void       XPM_Draw       (XPM *self, Surface *surface, PRectangle rc);
int        XPM_GetHeight  (const XPM *self);
int        XPM_GetWidth   (const XPM *self);
ColourRGBA XPM_PixelAt    (const XPM *self, int x, int y);

/* ── RGBAImage ───────────────────────────────────────────────────────── */
#define RGBA_BYTES_PER_PIXEL 4

typedef struct {
    int            height;
    int            width;
    float          scale;
    unsigned char *pixelBytes;   /* heap; width*height*4 bytes */
} RGBAImage;

RGBAImage *RGBAImage_create     (int width, int height, float scale,
                                  const unsigned char *pixels);
RGBAImage *RGBAImage_fromXPM    (const XPM *xpm);
void       RGBAImage_destroy    (RGBAImage *self);

int                  RGBAImage_GetHeight      (const RGBAImage *self);
int                  RGBAImage_GetWidth       (const RGBAImage *self);
float                RGBAImage_GetScale       (const RGBAImage *self);
float                RGBAImage_GetScaledHeight(const RGBAImage *self);
float                RGBAImage_GetScaledWidth (const RGBAImage *self);
int                  RGBAImage_CountBytes     (const RGBAImage *self);
const unsigned char *RGBAImage_Pixels         (const RGBAImage *self);
void                 RGBAImage_SetPixel       (RGBAImage *self, int x, int y,
                                               ColourRGBA colour);
void                 RGBAImage_BGRAFromRGBA   (unsigned char *pixelsBGRA,
                                               const unsigned char *pixelsRGBA,
                                               size_t count);

/* ── RGBAImageSet ────────────────────────────────────────────────────── */
typedef struct {
    int        id;
    RGBAImage *image;
} RGBAEntry;

typedef struct {
    RGBAEntry *entries;   /* heap; sorted by id */
    int        count;
    int        cap;
    int        height;    /* cached largest height; -1 = dirty */
    int        width;     /* cached largest width;  -1 = dirty */
} RGBAImageSet;

void        RIS_init    (RGBAImageSet *self);
void        RIS_destroy (RGBAImageSet *self);
void        RIS_Clear   (RGBAImageSet *self);
void        RIS_AddImage(RGBAImageSet *self, int ident, RGBAImage *image);
RGBAImage  *RIS_Get     (RGBAImageSet *self, int ident);
int         RIS_GetHeight(const RGBAImageSet *self);
int         RIS_GetWidth (const RGBAImageSet *self);

#endif /* XPM_C_H */
