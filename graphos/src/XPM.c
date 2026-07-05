/* Scintilla source code edit control
 * XPM.c — C translation of scintilla/src/XPM.cxx
 *
 * ── Translation summary ───────────────────────────────────────────────
 *
 *   std::vector<unsigned char>         →  unsigned char * + malloc/realloc/free
 *   std::vector<const char *>          →  const char ** + malloc/free (local only)
 *   std::map<int,unique_ptr<RGBAImage>>→  RGBAEntry[] sorted heap array
 *   std::fill(colourCodeTable,…,black) →  explicit loop
 *   pixels.resize / pixelBytes.resize  →  malloc / realloc
 *   pixels.empty()                     →  self->pixels == NULL
 *   reinterpret_cast<const char*const*>→  plain cast (char*)(textForm)
 *   anonymous-namespace helpers        →  static file-scope functions
 *   mutable int height/width           →  int height/width, -1 = dirty
 *   std::unique_ptr<RGBAImage>         →  RGBAImage * (owned by RGBAImageSet)
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "XPM.h"

/* ── File-private helpers ─────────────────────────────────────────────── */

static const char *NextField(const char *s) {
    while (*s == ' ') s++;
    while (*s && *s != ' ') s++;
    while (*s == ' ') s++;
    return s;
}

/* Data lines may be NUL- or '"'-terminated */
static size_t MeasureLength(const char *s) {
    size_t i = 0;
    while (s[i] && s[i] != '"') i++;
    return i;
}

static unsigned int ValueOfHex(char ch) {
    if (ch >= '0' && ch <= '9') return (unsigned int)(ch - '0');
    if (ch >= 'A' && ch <= 'F') return (unsigned int)(ch - 'A' + 10);
    if (ch >= 'a' && ch <= 'f') return (unsigned int)(ch - 'a' + 10);
    return 0;
}

static ColourRGBA ColourFromHex(const char *val) {
    const unsigned int r = ValueOfHex(val[0]) * 16 + ValueOfHex(val[1]);
    const unsigned int g = ValueOfHex(val[2]) * 16 + ValueOfHex(val[3]);
    const unsigned int b = ValueOfHex(val[4]) * 16 + ValueOfHex(val[5]);
    return ColourRGBA_make(r, g, b, COLOUR_MAX_BYTE);
}

/* Build a lines-form pointer array from a textual XPM blob.
 * Returns a heap-allocated array of pointers into textForm (not copies).
 * Caller must free() the returned pointer. *outCount is set to the count,
 * or 0 on error (returned pointer is NULL on error). */
static const char **LinesFormFromTextForm(const char *textForm, int *outCount) {
    *outCount = 0;
    int capacity = 64;
    const char **linesForm = malloc((size_t)capacity * sizeof(const char *));
    if (!linesForm) return NULL;

    int countQuotes = 0;
    int strings = 1;
    int count = 0;

    for (int j = 0; countQuotes < (2 * strings) && textForm[j] != '\0'; j++) {
        if (textForm[j] != '"') continue;

        if (countQuotes == 0) {
            const char *line0 = textForm + j + 1;
            line0 = NextField(line0);        /* skip width */
            strings += atoi(line0);          /* + height lines */
            line0 = NextField(line0);
            strings += atoi(line0);          /* + colour lines */
        }

        if (countQuotes / 2 >= strings) break; /* bad dimensions */

        if ((countQuotes & 1) == 0) {
            if (count == capacity) {
                capacity *= 2;
                const char **buf = realloc(linesForm,
                                           (size_t)capacity * sizeof(const char *));
                if (!buf) { free(linesForm); return NULL; }
                linesForm = buf;
            }
            linesForm[count++] = textForm + j + 1;
        }
        countQuotes++;
    }

    /* Validate: must have consumed exactly the right number of strings */
    if (count == 0 || count < strings) {
        free(linesForm);
        return NULL;
    }

    *outCount = count;
    return linesForm;
}

/* ── XPM ──────────────────────────────────────────────────────────────── */

static ColourRGBA XPM_ColourFromCode(const XPM *self, int ch) {
    return self->colourCodeTable[(unsigned char)ch];
}

static void XPM_FillRun(const XPM *self, Surface *surface,
                          int code, int startX, int y, int x) {
    if (code != (unsigned char)self->codeTransparent && startX != x) {
        const PRectangle rc = PRectangle_FromInts(startX, y, x, y + 1);
        Surface_FillRectangle(surface, rc, XPM_ColourFromCode(self, code));
    }
}

void XPM_init_text(XPM *self, const char *textForm) {
    if (memcmp(textForm, "/* X", 4) == 0 &&
        memcmp(textForm, "/* XPM */", 9) == 0) {
        int count = 0;
        const char **linesForm = LinesFormFromTextForm(textForm, &count);
        if (linesForm && count > 0) {
            XPM_init_lines(self, linesForm);
        }
        free(linesForm);
    } else {
        XPM_init_lines(self, (const char *const *)textForm);
    }
}

void XPM_init_lines(XPM *self, const char *const *linesForm) {
    self->height         = 1;
    self->width          = 1;
    self->nColours       = 1;
    self->codeTransparent = ' ';
    free(self->pixels);
    self->pixels = NULL;

    if (!linesForm) return;

    /* Initialise colour table to opaque black */
    for (int i = 0; i < 256; i++)
        self->colourCodeTable[i] = ColourRGBA_black;

    const char *line0 = linesForm[0];
    self->width  = atoi(line0);
    line0 = NextField(line0);
    self->height = atoi(line0);
    line0 = NextField(line0);
    self->nColours = atoi(line0);
    line0 = NextField(line0);
    if (atoi(line0) != 1) return;  /* only 1 char/pixel supported */

    self->pixels = malloc((size_t)(self->width * self->height));
    if (!self->pixels) return;
    memset(self->pixels, 0, (size_t)(self->width * self->height));

    /* Parse colour definitions */
    for (int c = 0; c < self->nColours; c++) {
        const char *colourDef = linesForm[c + 1];
        const char code = colourDef[0];
        colourDef += 4;
        ColourRGBA colour = ColourRGBA_make(0, 0, 0, 0); /* transparent */
        if (*colourDef == '#') {
            colour = ColourFromHex(colourDef + 1);
        } else {
            self->codeTransparent = code;
        }
        self->colourCodeTable[(unsigned char)code] = colour;
    }

    /* Copy pixel data */
    for (int y = 0; y < self->height; y++) {
        const char *lform = linesForm[y + self->nColours + 1];
        const size_t len  = MeasureLength(lform);
        for (size_t x = 0; x < len; x++)
            self->pixels[y * self->width + x] = (unsigned char)lform[x];
    }
}

void XPM_destroy(XPM *self) {
    free(self->pixels);
    self->pixels = NULL;
}

void XPM_Draw(XPM *self, Surface *surface, PRectangle rc) {
    if (!self->pixels) return;
    const int startY = (int)(rc.top  + (PRectangle_Height(rc) - self->height) / 2);
    const int startX = (int)(rc.left + (PRectangle_Width(rc)  - self->width)  / 2);
    for (int y = 0; y < self->height; y++) {
        int prevCode   = 0;
        int xStartRun  = 0;
        for (int x = 0; x < self->width; x++) {
            const int code = self->pixels[y * self->width + x];
            if (code != prevCode) {
                XPM_FillRun(self, surface, prevCode,
                             startX + xStartRun, startY + y, startX + x);
                xStartRun = x;
                prevCode  = code;
            }
        }
        XPM_FillRun(self, surface, prevCode,
                     startX + xStartRun, startY + y, startX + self->width);
    }
}

int XPM_GetHeight(const XPM *self) { return self->height; }
int XPM_GetWidth (const XPM *self) { return self->width;  }

ColourRGBA XPM_PixelAt(const XPM *self, int x, int y) {
    if (!self->pixels || x < 0 || x >= self->width || y < 0 || y >= self->height)
        return ColourRGBA_make(0, 0, 0, 0); /* transparent black */
    return XPM_ColourFromCode(self, self->pixels[y * self->width + x]);
}

/* ── RGBAImage ────────────────────────────────────────────────────────── */

RGBAImage *RGBAImage_create(int width, int height, float scale,
                              const unsigned char *pixels) {
    RGBAImage *self = malloc(sizeof(RGBAImage));
    if (!self) return NULL;
    self->width  = width;
    self->height = height;
    self->scale  = scale;
    const int nbytes = width * height * RGBA_BYTES_PER_PIXEL;
    self->pixelBytes = malloc((size_t)nbytes);
    if (!self->pixelBytes) { free(self); return NULL; }
    if (pixels)
        memcpy(self->pixelBytes, pixels, (size_t)nbytes);
    else
        memset(self->pixelBytes, 0, (size_t)nbytes);
    return self;
}

RGBAImage *RGBAImage_fromXPM(const XPM *xpm) {
    RGBAImage *self = RGBAImage_create(xpm->width, xpm->height, 1.0f, NULL);
    if (!self) return NULL;
    for (int y = 0; y < xpm->height; y++)
        for (int x = 0; x < xpm->width; x++)
            RGBAImage_SetPixel(self, x, y, XPM_PixelAt(xpm, x, y));
    return self;
}

void RGBAImage_destroy(RGBAImage *self) {
    if (!self) return;
    free(self->pixelBytes);
    free(self);
}

int   RGBAImage_GetHeight      (const RGBAImage *self) { return self->height; }
int   RGBAImage_GetWidth       (const RGBAImage *self) { return self->width;  }
float RGBAImage_GetScale       (const RGBAImage *self) { return self->scale;  }
float RGBAImage_GetScaledHeight(const RGBAImage *self) { return (float)self->height / self->scale; }
float RGBAImage_GetScaledWidth (const RGBAImage *self) { return (float)self->width  / self->scale; }
int   RGBAImage_CountBytes     (const RGBAImage *self) { return self->width * self->height * RGBA_BYTES_PER_PIXEL; }

const unsigned char *RGBAImage_Pixels(const RGBAImage *self) {
    return self->pixelBytes;
}

void RGBAImage_SetPixel(RGBAImage *self, int x, int y, ColourRGBA colour) {
    unsigned char *pixel = self->pixelBytes + (y * self->width + x) * RGBA_BYTES_PER_PIXEL;
    pixel[0] = ColourRGBA_GetRed  (colour);
    pixel[1] = ColourRGBA_GetGreen(colour);
    pixel[2] = ColourRGBA_GetBlue (colour);
    pixel[3] = ColourRGBA_GetAlpha(colour);
}

static unsigned char AlphaMultiplied(unsigned char value, unsigned char alpha) {
    return (unsigned char)((value * alpha / UCHAR_MAX) & 0xFFu);
}

void RGBAImage_BGRAFromRGBA(unsigned char *pixelsBGRA,
                              const unsigned char *pixelsRGBA, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const unsigned char alpha = pixelsRGBA[3];
        pixelsBGRA[2] = AlphaMultiplied(pixelsRGBA[0], alpha);
        pixelsBGRA[1] = AlphaMultiplied(pixelsRGBA[1], alpha);
        pixelsBGRA[0] = AlphaMultiplied(pixelsRGBA[2], alpha);
        pixelsBGRA[3] = alpha;
        pixelsRGBA += RGBA_BYTES_PER_PIXEL;
        pixelsBGRA += RGBA_BYTES_PER_PIXEL;
    }
}

/* ── RGBAImageSet ─────────────────────────────────────────────────────── */

void RIS_init(RGBAImageSet *self) {
    self->entries = NULL;
    self->count   = 0;
    self->cap     = 0;
    self->height  = -1;
    self->width   = -1;
}

void RIS_Clear(RGBAImageSet *self) {
    for (int i = 0; i < self->count; i++)
        RGBAImage_destroy(self->entries[i].image);
    self->count  = 0;
    self->height = -1;
    self->width  = -1;
}

void RIS_destroy(RGBAImageSet *self) {
    RIS_Clear(self);
    free(self->entries);
    self->entries = NULL;
    self->cap     = 0;
}

void RIS_AddImage(RGBAImageSet *self, int ident, RGBAImage *image) {
    /* Replace existing entry if id already present */
    for (int i = 0; i < self->count; i++) {
        if (self->entries[i].id == ident) {
            RGBAImage_destroy(self->entries[i].image);
            self->entries[i].image = image;
            self->height = -1;
            self->width  = -1;
            return;
        }
    }
    /* Grow array */
    if (self->count == self->cap) {
        int newCap = self->cap ? self->cap * 2 : 8;
        RGBAEntry *buf = realloc(self->entries,
                                  (size_t)newCap * sizeof(RGBAEntry));
        if (!buf) return;
        self->entries = buf;
        self->cap     = newCap;
    }
    self->entries[self->count].id    = ident;
    self->entries[self->count].image = image;
    self->count++;
    self->height = -1;
    self->width  = -1;
}

RGBAImage *RIS_Get(RGBAImageSet *self, int ident) {
    for (int i = 0; i < self->count; i++)
        if (self->entries[i].id == ident)
            return self->entries[i].image;
    return NULL;
}

int RIS_GetHeight(const RGBAImageSet *self) {
    if (self->height < 0) {
        int h = 0;
        for (int i = 0; i < self->count; i++) {
            const int ih = RGBAImage_GetHeight(self->entries[i].image);
            if (ih > h) h = ih;
        }
        /* cast away const for cache update — mirrors C++ mutable */
        ((RGBAImageSet *)self)->height = h;
    }
    return self->height > 0 ? self->height : 0;
}

int RIS_GetWidth(const RGBAImageSet *self) {
    if (self->width < 0) {
        int w = 0;
        for (int i = 0; i < self->count; i++) {
            const int iw = RGBAImage_GetWidth(self->entries[i].image);
            if (iw > w) w = iw;
        }
        ((RGBAImageSet *)self)->width = w;
    }
    return self->width > 0 ? self->width : 0;
}
