/* Scintilla source code edit control
 * Position.h — C translation of scintilla/src/Position.h
 *
 * C++ original:
 *   namespace Sci {
 *       typedef ptrdiff_t Position;
 *       typedef ptrdiff_t Line;
 *       inline constexpr Position invalidPosition = -1;
 *   }
 *
 * Translation:
 *   - namespace Sci → Sci_ prefix
 *   - inline constexpr → #define  (compile-time constant, no linkage needed)
 *   - Both typedefs kept as ptrdiff_t so pointer arithmetic is safe
 */

#ifndef POSITION_C_H
#define POSITION_C_H

#include <stddef.h>   /* ptrdiff_t */

typedef ptrdiff_t SciPosition;
typedef ptrdiff_t SciLine;

#define SCI_INVALID_POSITION ((SciPosition)(-1))

#endif /* POSITION_C_H */
