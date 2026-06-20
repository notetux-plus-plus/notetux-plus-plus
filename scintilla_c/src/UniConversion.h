/* Scintilla source code edit control
 * UniConversion.h — C translation of scintilla/src/UniConversion.h
 *                   (subset used by CellBuffer.c)
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   constexpr bool → static inline int  (returns 0/1)
 *   constexpr int  → enum / #define
 *   namespace Scintilla::Internal → removed; names kept as-is
 *   std::string_view  → (const char *, size_t)
 *   std::wstring_view → not translated (not needed here)
 *
 * Functions that are not inline constexpr (e.g. UTF8IsValid) are declared
 * here and defined in UniConversion.c.
 */

#ifndef UNICONVERSION_C_H
#define UNICONVERSION_C_H

#include <stddef.h>
#include <stdint.h>

#define UTF8MaxBytes 4

enum { UTF8MaskWidth = 0x7, UTF8MaskInvalid = 0x8 };
#define UTF8SeparatorLength 3
#define UTF8NELLength       2

/* Trail byte: 0x80..0xBF */
static inline int UTF8IsTrailByte(unsigned char ch) {
    return (ch >= 0x80) && (ch < 0xc0);
}

/* First byte of a multi-byte sequence */
static inline int UTF8IsFirstByte(unsigned char ch) {
    return (ch >= 0xc2) && (ch <= 0xf4);
}

/* ASCII: bit 7 clear */
static inline int UTF8IsAscii_uc(unsigned char ch) {
    return ch < 0x80;
}
static inline int UTF8IsAscii(char ch) {
    return (unsigned char)ch < 0x80;
}

/* Line / Paragraph separator: U+2028 \xe2\x80\xa8 or U+2029 \xe2\x80\xa9 */
static inline int UTF8IsSeparator(const unsigned char *us) {
    return (us[0] == 0xe2) && (us[1] == 0x80) && ((us[2] == 0xa8) || (us[2] == 0xa9));
}

/* NEL: U+0085 \xc2\x85 */
static inline int UTF8IsNEL(const unsigned char *us) {
    return (us[0] == 0xc2) && (us[1] == 0x85);
}

/* Three-character UTF-8 multibyte line end (LS, PS, or NEL) */
static inline int UTF8IsMultibyteLineEnd(unsigned char ch0, unsigned char ch1, unsigned char ch2) {
    return ((ch0 == 0xe2) && (ch1 == 0x80) && ((ch2 == 0xa8) || (ch2 == 0xa9))) ||
           ((ch1 == 0xc2) && (ch2 == 0x85));
}

/* Extern table and non-inline functions (defined in UniConversion.c) */
extern const unsigned char UTF8BytesOfLead[256];

int  UTF8Classify(const unsigned char *us, size_t len);
int  UTF8Classify_char(const char *s, size_t len);
int  UTF8IsValid(const char *s, size_t len);

#endif /* UNICONVERSION_C_H */
