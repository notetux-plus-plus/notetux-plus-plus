/* Scintilla source code edit control
 * CharClassify.c — C translation of scintilla/src/CharClassify.cxx
 *
 * Original C++ by Neil Hodgson <neilh@scintilla.org>.
 *
 * ── Translation map ───────────────────────────────────────────────────
 *
 *  #include <cstdlib>          →  #include <stdlib.h>
 *  #include <cassert>          →  #include <assert.h>
 *  #include <stdexcept>        →  (removed — C has no exceptions)
 *  using namespace Sci::Intl;  →  (removed — no namespaces in C)
 *
 *  CharClassify::CharClassify()
 *  → CharClassify_init(CharClassify *self)
 *    The C++ constructor initialised charClass{} (zero-init) then called
 *    SetDefaultCharClasses(true).  In C, the caller is responsible for
 *    providing storage (stack or heap); CharClassify_init handles the rest.
 *    Zero-init of the array is explicit here via memset so the function
 *    is safe even if the caller passes uninitialised memory.
 *
 *  void CharClassify::SetDefaultCharClasses(bool includeWordClass)
 *  → void CharClassify_SetDefaultCharClasses(CharClassify *self, int includeWordClass)
 *    bool → int  (C99 _Bool exists but int is idiomatic for flag parameters)
 *    CharacterClass::newLine  → CharacterClass_newLine   (no scoped enum)
 *    CharacterClass::space    → CharacterClass_space
 *    CharacterClass::word     → CharacterClass_word
 *    CharacterClass::punctuation → CharacterClass_punctuation
 *    IsControl / IsAlphaNumeric  → Sci_IsControl / Sci_IsAlphaNumeric
 *      (prefixed versions from CharacterType.h)
 *
 *  void CharClassify::SetCharClasses(...)
 *  → void CharClassify_SetCharClasses(CharClassify *self, ...)
 *    Identical logic; "this->" replaced by "self->".
 *
 *  int CharClassify::GetCharsOfClass(...) const noexcept
 *  → int CharClassify_GetCharsOfClass(const CharClassify *self, ...)
 *    const method → const pointer parameter.
 *    noexcept → removed.
 *    static_cast<unsigned char>(ch) → (unsigned char)(ch)  (C cast)
 *
 * ─────────────────────────────────────────────────────────────────────
 */

#include <stdlib.h>
#include <assert.h>
#include <string.h>   /* memset */

#include "CharacterType.h"
#include "CharClassify.h"

/* CharClassify() constructor
 *
 * C++ original:
 *   CharClassify::CharClassify() : charClass{} {
 *       SetDefaultCharClasses(true);
 *   }
 *
 * charClass{} is member initialisation — it zero-inits the array before
 * the constructor body runs.  We replicate this with memset so that
 * CharClassify_init is safe to call on any (even uninitialised) storage.
 */
void CharClassify_init(CharClassify *self) {
    assert(self);
    memset(self->charClass, 0, sizeof self->charClass);
    CharClassify_SetDefaultCharClasses(self, 1 /* true */);
}

/* void CharClassify::SetDefaultCharClasses(bool includeWordClass)
 *
 * Iterates all 256 characters and assigns a default class to each.
 * The logic is identical to the C++ version; only spelling changes:
 *   ch == '\r' || ch == '\n'  → CharacterClass_newLine
 *   IsControl(ch) || ch==' '  → CharacterClass_space
 *   includeWordClass &&
 *     (ch>=0x80 || IsAlphaNumeric(ch) || ch=='_') → CharacterClass_word
 *   otherwise                 → CharacterClass_punctuation
 */
void CharClassify_SetDefaultCharClasses(CharClassify *self, int includeWordClass) {
    assert(self);
    for (int ch = 0; ch < SCI_CHAR_CLASSIFY_MAX; ch++) {
        if (ch == '\r' || ch == '\n') {
            self->charClass[ch] = (unsigned char)CharacterClass_newLine;
        } else if (Sci_IsControl(ch) || ch == ' ') {
            self->charClass[ch] = (unsigned char)CharacterClass_space;
        } else if (includeWordClass && (ch >= 0x80 || Sci_IsAlphaNumeric(ch) || ch == '_')) {
            self->charClass[ch] = (unsigned char)CharacterClass_word;
        } else {
            self->charClass[ch] = (unsigned char)CharacterClass_punctuation;
        }
    }
}

/* void CharClassify::SetCharClasses(const unsigned char *chars,
 *                                    CharacterClass newCharClass)
 *
 * Walks the NUL-terminated byte string chars and stamps every listed
 * character with newCharClass in the table.
 */
void CharClassify_SetCharClasses(CharClassify *self,
                                  const unsigned char *chars,
                                  CharacterClass newCharClass) {
    assert(self);
    if (chars) {
        while (*chars) {
            self->charClass[*chars] = (unsigned char)newCharClass;
            chars++;
        }
    }
}

/* int CharClassify::GetCharsOfClass(CharacterClass characterClass,
 *                                    unsigned char *buffer) const noexcept
 *
 * Counts (and optionally collects) all characters that belong to
 * characterClass.  Iterates backwards so the output buffer is in
 * ascending character order (the original does the same).
 *
 * C++ used static_cast<unsigned char>(ch) to write into the buffer.
 * In C we write (unsigned char)(ch) — same effect, different syntax.
 */
int CharClassify_GetCharsOfClass(const CharClassify *self,
                                  CharacterClass characterClass,
                                  unsigned char *buffer) {
    assert(self);
    int count = 0;
    for (int ch = SCI_CHAR_CLASSIFY_MAX - 1; ch >= 0; --ch) {
        if (self->charClass[ch] == (unsigned char)characterClass) {
            ++count;
            if (buffer) {
                *buffer = (unsigned char)ch;
                buffer++;
            }
        }
    }
    return count;
}
