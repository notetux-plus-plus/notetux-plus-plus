/* test_CharClassify.c — verify the C translation of CharClassify.
 *
 * Compile & run:
 *   gcc -std=c11 -Wall -Wextra -I. \
 *       CharacterType.c CharClassify.c test_CharClassify.c -o test_cc && ./test_cc
 */

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "CharClassify.h"
#include "CharacterType.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else          { printf("pass: %s\n", msg); } \
} while(0)

int main(void) {
    CharClassify cc;
    CharClassify_init(&cc);

    /* ── Default classification ──────────────────────────────────── */

    CHECK(CharClassify_GetClass(&cc, '\n') == CharacterClass_newLine,
          "LF is newLine");
    CHECK(CharClassify_GetClass(&cc, '\r') == CharacterClass_newLine,
          "CR is newLine");
    CHECK(CharClassify_GetClass(&cc, ' ')  == CharacterClass_space,
          "space is space");
    CHECK(CharClassify_GetClass(&cc, '\t') == CharacterClass_space,
          "tab is space (control character)");
    CHECK(CharClassify_GetClass(&cc, 'a')  == CharacterClass_word,
          "'a' is word");
    CHECK(CharClassify_GetClass(&cc, 'Z')  == CharacterClass_word,
          "'Z' is word");
    CHECK(CharClassify_GetClass(&cc, '0')  == CharacterClass_word,
          "'0' is word");
    CHECK(CharClassify_GetClass(&cc, '_')  == CharacterClass_word,
          "'_' is word");
    CHECK(CharClassify_GetClass(&cc, '.')  == CharacterClass_punctuation,
          "'.' is punctuation");
    CHECK(CharClassify_GetClass(&cc, '+')  == CharacterClass_punctuation,
          "'+' is punctuation");

    /* High bytes (>= 0x80) default to word class */
    CHECK(CharClassify_GetClass(&cc, 0x80) == CharacterClass_word,
          "0x80 is word");
    CHECK(CharClassify_GetClass(&cc, 0xFF) == CharacterClass_word,
          "0xFF is word");

    /* ── IsWord helper ───────────────────────────────────────────── */

    CHECK(CharClassify_IsWord(&cc, 'a'),  "IsWord('a')");
    CHECK(!CharClassify_IsWord(&cc, '.'), "!IsWord('.')");
    CHECK(!CharClassify_IsWord(&cc, ' '), "!IsWord(' ')");

    /* ── SetCharClasses ──────────────────────────────────────────── */

    const unsigned char custom[] = { '.', '-', '>', '\0' };
    CharClassify_SetCharClasses(&cc, custom, CharacterClass_word);
    CHECK(CharClassify_GetClass(&cc, '.') == CharacterClass_word,
          "'.' reclassified to word");
    CHECK(CharClassify_GetClass(&cc, '-') == CharacterClass_word,
          "'-' reclassified to word");
    CHECK(CharClassify_GetClass(&cc, '>') == CharacterClass_word,
          "'>' reclassified to word");
    CHECK(CharClassify_GetClass(&cc, '+') == CharacterClass_punctuation,
          "'+' still punctuation after SetCharClasses");

    /* ── GetCharsOfClass (count-only mode) ───────────────────────── */

    /* After reclassifying '.', '-', '>', count of punctuation chars
     * should be 3 less than the default.  We just check it is > 0. */
    int n = CharClassify_GetCharsOfClass(&cc, CharacterClass_punctuation, NULL);
    CHECK(n > 0, "GetCharsOfClass(punctuation) returns positive count");

    /* Collect and verify round-trip */
    unsigned char buf[256];
    int m = CharClassify_GetCharsOfClass(&cc, CharacterClass_word, buf);
    CHECK(m > 0, "GetCharsOfClass(word) returns positive count");
    /* Every collected char should actually be word class */
    int ok = 1;
    for (int i = 0; i < m; i++) {
        if (CharClassify_GetClass(&cc, buf[i]) != CharacterClass_word) {
            ok = 0; break;
        }
    }
    CHECK(ok, "All chars returned by GetCharsOfClass(word) are word class");

    /* ── SetDefaultCharClasses(false) ────────────────────────────── */

    CharClassify_SetDefaultCharClasses(&cc, 0 /* false */);
    /* With includeWordClass=false, alphanumeric chars become punctuation */
    CHECK(CharClassify_GetClass(&cc, 'a') == CharacterClass_punctuation,
          "includeWordClass=0: 'a' is punctuation");
    CHECK(CharClassify_GetClass(&cc, '_') == CharacterClass_punctuation,
          "includeWordClass=0: '_' is punctuation");
    /* But newline and space still classified correctly */
    CHECK(CharClassify_GetClass(&cc, '\n') == CharacterClass_newLine,
          "includeWordClass=0: LF still newLine");
    CHECK(CharClassify_GetClass(&cc, ' ')  == CharacterClass_space,
          "includeWordClass=0: space still space");

    /* ── Sci_CompareCaseInsensitive ──────────────────────────────── */

    CHECK(Sci_CompareCaseInsensitive("abc", "ABC") == 0, "CmpCI: abc==ABC");
    CHECK(Sci_CompareCaseInsensitive("abc", "abd")  < 0, "CmpCI: abc<abd");
    CHECK(Sci_CompareCaseInsensitive("abd", "abc")  > 0, "CmpCI: abd>abc");
    CHECK(Sci_CompareCaseInsensitive("ab",  "abc")  < 0, "CmpCI: ab<abc");
    CHECK(Sci_CompareCaseInsensitive("abc", "ab")   > 0, "CmpCI: abc>ab");
    CHECK(Sci_CompareCaseInsensitive("",    "")    == 0, "CmpCI: empty==empty");

    CHECK(Sci_CompareNCaseInsensitive("abcX", "ABCY", 3) == 0, "CmpNCI: first 3 equal");
    CHECK(Sci_CompareNCaseInsensitive("abc",  "abd",  3)  < 0, "CmpNCI: abc<abd");

    printf("\n%s — %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
