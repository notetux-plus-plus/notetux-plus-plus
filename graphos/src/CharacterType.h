/* Scintilla source code edit control
 * CharacterType.h — C translation of scintilla/src/CharacterType.h
 *
 * Original C++ by Neil Hodgson <neilh@scintilla.org>.
 * C translation: same logic, no namespaces, no templates, no constexpr.
 *
 * Translation notes:
 *   - namespace Scintilla::Internal removed; all names prefixed with Sci_
 *   - constexpr bool → static inline int  (C has no bool before C99 _Bool;
 *     we return int and treat non-zero as true throughout)
 *   - template<typename T> MakeUpperCase / MakeLowerCase → int versions only
 *     (T was always char or int in all call sites)
 *   - Overloaded IsADigit(ch, base) → Sci_IsADigitBase(ch, base)
 */

#ifndef CHARACTERTYPE_C_H
#define CHARACTERTYPE_C_H

#include <stddef.h>   /* size_t */

/* ── Character predicates ─────────────────────────────────────────────
 * All take int so they accept both plain char and unsigned char values
 * without casting at the call site.  Behaviour is defined for 0–255
 * and EOF (-1).
 * ───────────────────────────────────────────────────────────────────── */

/* Space: ASCII space plus HT, LF, VT, FF, CR (0x09–0x0D). */
static inline int Sci_IsASpace(int ch) {
    return (ch == ' ') || ((ch >= 0x09) && (ch <= 0x0d));
}

static inline int Sci_IsSpaceOrTab(int ch) {
    return (ch == ' ') || (ch == '\t');
}

/* C0 control characters (0x00–0x1F) and DEL (0x7F). */
static inline int Sci_IsControl(int ch) {
    return ((ch >= 0) && (ch <= 0x1F)) || (ch == 0x7F);
}

static inline int Sci_IsEOLCharacter(int ch) {
    return ch == '\r' || ch == '\n';
}

/* Break-space: C0 control characters and ASCII space (used for text
 * breaking; DEL omitted deliberately — see original comment). */
static inline int Sci_IsBreakSpace(int ch) {
    return ch >= 0 && ch <= ' ';
}

static inline int Sci_IsADigit(int ch) {
    return (ch >= '0') && (ch <= '9');
}

/* Digit in an arbitrary base (2–16). */
static inline int Sci_IsADigitBase(int ch, int base) {
    if (base <= 10) {
        return (ch >= '0') && (ch < '0' + base);
    } else {
        return ((ch >= '0') && (ch <= '9')) ||
               ((ch >= 'A') && (ch < 'A' + base - 10)) ||
               ((ch >= 'a') && (ch < 'a' + base - 10));
    }
}

static inline int Sci_IsASCII(int ch) {
    return (ch >= 0) && (ch < 0x80);
}

static inline int Sci_IsLowerCase(int ch) {
    return (ch >= 'a') && (ch <= 'z');
}

static inline int Sci_IsUpperCase(int ch) {
    return (ch >= 'A') && (ch <= 'Z');
}

static inline int Sci_IsUpperOrLowerCase(int ch) {
    return Sci_IsUpperCase(ch) || Sci_IsLowerCase(ch);
}

static inline int Sci_IsAlphaNumeric(int ch) {
    return ((ch >= '0') && (ch <= '9')) ||
           ((ch >= 'a') && (ch <= 'z')) ||
           ((ch >= 'A') && (ch <= 'Z'));
}

static inline int Sci_IsPunctuation(int ch) {
    switch (ch) {
        case '!': case '"': case '#': case '$': case '%':
        case '&': case '\'': case '(': case ')': case '*':
        case '+': case ',': case '-': case '.': case '/':
        case ':': case ';': case '<': case '=': case '>':
        case '?': case '@': case '[': case '\\': case ']':
        case '^': case '_': case '`': case '{': case '|':
        case '}': case '~':
            return 1;
        default:
            return 0;
    }
}

/* ── Case conversion ──────────────────────────────────────────────────
 * C++ used templates so these worked on both char and int.  In C we
 * just use int — all call sites that passed char are implicitly
 * promoted to int anyway.
 * ───────────────────────────────────────────────────────────────────── */

static inline int Sci_MakeUpperCase(int ch) {
    return (ch >= 'a' && ch <= 'z') ? ch - 'a' + 'A' : ch;
}

static inline int Sci_MakeLowerCase(int ch) {
    return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch;
}

/* ── Case-insensitive string comparison ───────────────────────────────
 * Declarations only; definitions in CharacterType.c
 * ───────────────────────────────────────────────────────────────────── */

int Sci_CompareCaseInsensitive(const char *a, const char *b);
int Sci_CompareNCaseInsensitive(const char *a, const char *b, size_t len);

#endif /* CHARACTERTYPE_C_H */
