/* Scintilla source code edit control
 * CharClassify.h — C translation of scintilla/src/CharClassify.h
 *
 * Original C++ by Neil Hodgson <neilh@scintilla.org>.
 *
 * Translation notes:
 *
 *   enum class CharacterClass : unsigned char { space, newLine, word, punctuation }
 *   ─────────────────────────────────────────────────────────────────────────────
 *   C has no scoped enums.  We use a plain typedef enum with explicit values.
 *   The underlying type is unsigned char in C++ — in C we can't enforce this
 *   in the enum declaration, but CharClassify stores it as unsigned char in
 *   the array, so the effective storage size is identical.
 *
 *   class CharClassify
 *   ─────────────────────────────────────────────────────────────────────────────
 *   No virtual methods → no vtable struct needed.
 *   The class becomes a plain struct.  All methods become free functions that
 *   take a pointer to the struct as their first argument (the implicit "this").
 *
 *   static constexpr int maxChar = 256
 *   ─────────────────────────────────────────────────────────────────────────────
 *   → #define SCI_CHAR_CLASSIFY_MAX 256
 *   A named constant is cleaner than a magic 256 scattered in the code.
 *
 *   Inline methods GetClass / IsWord
 *   ─────────────────────────────────────────────────────────────────────────────
 *   C++ inline class methods → static inline free functions in the header.
 *   They are static so multiple translation units can include this header
 *   without triggering duplicate-symbol linker errors.
 *
 *   const noexcept
 *   ─────────────────────────────────────────────────────────────────────────────
 *   noexcept: dropped (C has no exceptions).
 *   const method: becomes const CharClassify *self parameter.
 */

#ifndef CHARCLASSIFY_C_H
#define CHARCLASSIFY_C_H

/* ── CharacterClass ───────────────────────────────────────────────────
 *
 * C++ original:
 *   enum class CharacterClass : unsigned char { space, newLine, word, punctuation };
 *
 * The scoped enum prevents accidental mixing with integers.  In C we
 * approximate this by using a typedef so the type name is distinct, and
 * by never mixing CharacterClass values with raw integers in the API.
 * ───────────────────────────────────────────────────────────────────── */
typedef enum {
    CharacterClass_space       = 0,
    CharacterClass_newLine     = 1,
    CharacterClass_word        = 2,
    CharacterClass_punctuation = 3
} CharacterClass;

/* ── CharClassify ─────────────────────────────────────────────────────
 *
 * C++ original:
 *   class CharClassify {
 *   private:
 *       static constexpr int maxChar = 256;
 *       CharacterClass charClass[maxChar];
 *   public:
 *       CharClassify();
 *       void SetDefaultCharClasses(bool includeWordClass);
 *       void SetCharClasses(const unsigned char *chars, CharacterClass newCharClass);
 *       int  GetCharsOfClass(CharacterClass cc, unsigned char *buffer) const noexcept;
 *       CharacterClass GetClass(unsigned char ch) const noexcept { return charClass[ch]; }
 *       bool IsWord(unsigned char ch) const noexcept { ... }
 *   };
 *
 * C translation: plain struct + free functions.
 * ───────────────────────────────────────────────────────────────────── */

#define SCI_CHAR_CLASSIFY_MAX 256

typedef struct {
    /* All 256 ASCII/Latin-1 characters classified.  Stored as unsigned char
     * to match the original C++ storage size (enum class : unsigned char). */
    unsigned char charClass[SCI_CHAR_CLASSIFY_MAX];
} CharClassify;

/* Constructor equivalent — call this instead of CharClassify() */
void CharClassify_init(CharClassify *self);

/* void SetDefaultCharClasses(bool includeWordClass)
 * Resets the table to defaults.  Pass 1 (true) to mark alphanumeric and
 * underscore characters as word characters; pass 0 to mark them as
 * punctuation instead (used in some regex contexts). */
void CharClassify_SetDefaultCharClasses(CharClassify *self, int includeWordClass);

/* void SetCharClasses(const unsigned char *chars, CharacterClass newCharClass)
 * Applies newCharClass to every character in the NUL-terminated string chars. */
void CharClassify_SetCharClasses(CharClassify *self,
                                  const unsigned char *chars,
                                  CharacterClass newCharClass);

/* int GetCharsOfClass(CharacterClass characterClass, unsigned char *buffer) const
 * Returns the count of characters with the given class.
 * If buffer != NULL, writes those characters into buffer (caller provides space). */
int CharClassify_GetCharsOfClass(const CharClassify *self,
                                  CharacterClass characterClass,
                                  unsigned char *buffer);

/* ── Inline accessors ─────────────────────────────────────────────────
 *
 * C++ originals were inline class methods.  In C, static inline free
 * functions in the header give the same one-instruction expansion at
 * the call site with no call overhead.
 * ───────────────────────────────────────────────────────────────────── */

/* CharacterClass GetClass(unsigned char ch) const noexcept */
static inline CharacterClass CharClassify_GetClass(const CharClassify *self,
                                                    unsigned char ch) {
    return (CharacterClass)self->charClass[ch];
}

/* bool IsWord(unsigned char ch) const noexcept */
static inline int CharClassify_IsWord(const CharClassify *self, unsigned char ch) {
    return self->charClass[ch] == CharacterClass_word;
}

#endif /* CHARCLASSIFY_C_H */
