/* Scintilla source code edit control
 * ScintillaTypes.h — C translation of scintilla/include/ScintillaTypes.h
 *                    (subset: only what CellBuffer and its dependencies need)
 *
 * C++ originals used:
 *
 *   enum class LineEndType { Default = 0, Unicode = 1 };
 *   enum class LineCharacterIndexType { None = 0, Utf32 = 1, Utf16 = 2 };
 *
 *   constexpr bool FlagSet(T value, T test) {
 *       return (static_cast<int>(value) & static_cast<int>(test)) != 0;
 *   }
 *
 *   constexpr LineEndType operator&(LineEndType a, LineEndType b) noexcept
 *   constexpr LineCharacterIndexType operator|(LineCharacterIndexType a, b)
 *   constexpr LineCharacterIndexType operator&(LineCharacterIndexType a, b)
 *
 * Translation notes:
 *   - enum class → typedef enum  (no scoping; names prefixed to avoid clashes)
 *   - template FlagSet<T> → macro (works on any integer type)
 *   - operator overloads → macros (bitwise AND / OR on the underlying int)
 *     Using macros rather than inline functions keeps the header dependency-free
 *     and avoids needing to cast through int explicitly at every call site.
 */

#ifndef SCINTILLATYPES_C_H
#define SCINTILLATYPES_C_H

/* ── LineEndType ──────────────────────────────────────────────────────
 *
 * C++ original:
 *   enum class LineEndType { Default = 0, Unicode = 1 };
 * ───────────────────────────────────────────────────────────────────── */
typedef enum {
    LineEndType_Default = 0,
    LineEndType_Unicode = 1
} LineEndType;

/* ── LineCharacterIndexType ───────────────────────────────────────────
 *
 * C++ original:
 *   enum class LineCharacterIndexType { None = 0, Utf32 = 1, Utf16 = 2 };
 *
 * Used as a bit-field (values are combined with | and tested with &).
 * ───────────────────────────────────────────────────────────────────── */
typedef enum {
    LineCharacterIndexType_None  = 0,
    LineCharacterIndexType_Utf32 = 1,
    LineCharacterIndexType_Utf16 = 2
} LineCharacterIndexType;

/* ── Bitwise helpers ──────────────────────────────────────────────────
 *
 * C++ used operator overloads for | and & on these enum classes.
 * In C, enums are just ints, so the operators work natively.
 * We only need FlagSet, which replaces the template.
 *
 * C++ original:
 *   template <typename T>
 *   constexpr bool FlagSet(T value, T test) {
 *       return (static_cast<int>(value) & static_cast<int>(test)) != 0;
 *   }
 *
 * macro version: works on any integer-compatible type, no cast needed.
 * ───────────────────────────────────────────────────────────────────── */
#define SCI_FLAG_SET(value, test)  (((int)(value) & (int)(test)) != 0)

/* ── [[nodiscard]] ────────────────────────────────────────────────────
 *
 * C++ used [[nodiscard]] on several CellBuffer methods.
 * In C we use __attribute__((warn_unused_result)) for GCC/Clang.
 * ───────────────────────────────────────────────────────────────────── */
#if defined(__GNUC__) || defined(__clang__)
#  define SCI_NODISCARD __attribute__((warn_unused_result))
#else
#  define SCI_NODISCARD
#endif

#endif /* SCINTILLATYPES_C_H */
