/* Scintilla source code edit control
 * CharacterType.c — C translation of scintilla/src/CharacterType.cxx
 *
 * Original C++ by Neil Hodgson <neilh@scintilla.org>.
 *
 * Translation notes:
 *   - #include <cstdlib>/<cassert> → <stdlib.h>/<assert.h>
 *   - namespace Scintilla::Internal removed
 *   - noexcept removed (C has no exceptions)
 *   - MakeUpperCase<char> → Sci_MakeUpperCase (defined inline in header)
 *   - Function names prefixed with Sci_
 */

#include <stdlib.h>
#include <stddef.h>

#include "CharacterType.h"

int Sci_CompareCaseInsensitive(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) {
            char upperA = (char)Sci_MakeUpperCase((unsigned char)*a);
            char upperB = (char)Sci_MakeUpperCase((unsigned char)*b);
            if (upperA != upperB)
                return upperA - upperB;
        }
        a++;
        b++;
    }
    /* One or both strings ended — compare the terminators */
    return (unsigned char)*a - (unsigned char)*b;
}

int Sci_CompareNCaseInsensitive(const char *a, const char *b, size_t len) {
    while (*a && *b && len) {
        if (*a != *b) {
            char upperA = (char)Sci_MakeUpperCase((unsigned char)*a);
            char upperB = (char)Sci_MakeUpperCase((unsigned char)*b);
            if (upperA != upperB)
                return upperA - upperB;
        }
        a++;
        b++;
        len--;
    }
    if (len == 0)
        return 0;
    /* One or both strings ended */
    return (unsigned char)*a - (unsigned char)*b;
}
