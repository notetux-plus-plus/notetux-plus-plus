/* Scintilla source code edit control
 * UniConversion.c — C translation of scintilla/src/UniConversion.cxx
 *                   (subset needed by CellBuffer.c + CaseConvert.c)
 *
 * Translated functions:
 *   UTF8BytesOfLead[] table
 *   UTF8Classify(const unsigned char *, size_t)
 *   UTF8Classify_char(const char *, size_t)    [renamed to avoid C++ overload name]
 *   UTF8IsValid(const char *, size_t)
 *
 * Deliberately omitted (not needed by CellBuffer):
 *   UTF8Length, UTF8FromUTF16, UTF16Length, UTF16FromUTF8, WStringFromUTF8, etc.
 */

#include <stddef.h>
#include <stdint.h>
#include "UniConversion.h"

/* ── Lead-byte table ─────────────────────────────────────────────────
 *
 * UTF8BytesOfLead[b] = number of bytes in the UTF-8 sequence starting
 * with byte b.  Invalid lead bytes (0x80-0xBF, 0xC0-0xC1, 0xF5-0xFF)
 * map to 1 so they are treated as isolated bytes.
 */
const unsigned char UTF8BytesOfLead[256] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 00-0F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 10-1F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 20-2F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 30-3F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 40-4F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 50-5F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 60-6F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 70-7F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 80-8F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* 90-9F */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* A0-AF */
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* B0-BF */
    1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  /* C0-CF */
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  /* D0-DF */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /* E0-EF */
    4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  /* F0-FF */
};

/* ── UTF8Classify ────────────────────────────────────────────────────
 *
 * Returns (UTF8MaskInvalid | width) for invalid sequences,
 * or just width (1-4) for valid ones.
 *
 * C++ original: int UTF8Classify(const unsigned char *us, size_t len) noexcept
 */
int UTF8Classify(const unsigned char *us, size_t len) {
    if (us[0] < 0x80)
        return 1;

    const size_t byteCount = UTF8BytesOfLead[us[0]];
    if (byteCount == 1 || byteCount > len)
        return UTF8MaskInvalid | 1;

    if (!UTF8IsTrailByte(us[1]))
        return UTF8MaskInvalid | 1;

    switch (byteCount) {
    case 2:
        return 2;

    case 3:
        if (UTF8IsTrailByte(us[2])) {
            if ((us[0] == 0xe0) && ((us[1] & 0xe0) == 0x80))
                return UTF8MaskInvalid | 1;   /* overlong */
            if ((us[0] == 0xed) && ((us[1] & 0xe0) == 0xa0))
                return UTF8MaskInvalid | 1;   /* surrogate */
            if ((us[0] == 0xef) && (us[1] == 0xbf) && (us[2] == 0xbe))
                return UTF8MaskInvalid | 3;   /* U+FFFE */
            if ((us[0] == 0xef) && (us[1] == 0xbf) && (us[2] == 0xbf))
                return UTF8MaskInvalid | 3;   /* U+FFFF */
            if ((us[0] == 0xef) && (us[1] == 0xb7) &&
                (((us[2] & 0xf0) == 0x90) || ((us[2] & 0xf0) == 0xa0)))
                return UTF8MaskInvalid | 3;   /* U+FDD0..U+FDEF */
            return 3;
        }
        break;

    default: /* 4 */
        if (UTF8IsTrailByte(us[2]) && UTF8IsTrailByte(us[3])) {
            if (((us[1] & 0xf) == 0xf) && (us[2] == 0xbf) &&
                ((us[3] == 0xbe) || (us[3] == 0xbf)))
                return UTF8MaskInvalid | 4;   /* *FFFE or *FFFF */
            if (us[0] == 0xf4) {
                if (us[1] > 0x8f)
                    return UTF8MaskInvalid | 1;
            } else if ((us[0] == 0xf0) && ((us[1] & 0xf0) == 0x80)) {
                return UTF8MaskInvalid | 1;   /* overlong */
            }
            return 4;
        }
        break;
    }

    return UTF8MaskInvalid | 1;
}

/* char* overload: same, just cast */
int UTF8Classify_char(const char *s, size_t len) {
    return UTF8Classify((const unsigned char *)s, len);
}

/* ── UTF8IsValid ──────────────────────────────────────────────────────
 *
 * C++ original: bool UTF8IsValid(std::string_view svu8) noexcept
 */
int UTF8IsValid(const char *s, size_t len) {
    size_t remaining = len;
    while (remaining > 0) {
        const int utf8Status = UTF8Classify((const unsigned char *)s, remaining);
        if (utf8Status & UTF8MaskInvalid)
            return 0;
        const int lenChar = utf8Status & UTF8MaskWidth;
        s         += lenChar;
        remaining -= (size_t)lenChar;
    }
    return 1;
}

void UTF8FromUTF32Character(int uch, char *putf) {
    size_t k = 0;
    if (uch < 0x80) {
        putf[k++] = (char)uch;
    } else if (uch < 0x800) {
        putf[k++] = (char)(0xC0 | (uch >> 6));
        putf[k++] = (char)(0x80 | (uch & 0x3F));
    } else if (uch < 0x10000) {
        putf[k++] = (char)(0xE0 | (uch >> 12));
        putf[k++] = (char)(0x80 | ((uch >> 6) & 0x3F));
        putf[k++] = (char)(0x80 | (uch & 0x3F));
    } else {
        putf[k++] = (char)(0xF0 | (uch >> 18));
        putf[k++] = (char)(0x80 | ((uch >> 12) & 0x3F));
        putf[k++] = (char)(0x80 | ((uch >> 6)  & 0x3F));
        putf[k++] = (char)(0x80 | (uch & 0x3F));
    }
    putf[k] = '\0';
}
