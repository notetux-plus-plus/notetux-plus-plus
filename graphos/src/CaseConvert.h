/* Scintilla source code edit control
 * CaseConvert.h — C translation of scintilla/src/CaseConvert.h
 *
 * ── C++ → C translation notes ────────────────────────────────────────
 *
 *   enum class CaseConversion { fold, upper, lower }
 *     →  typedef enum { CASE_FOLD=0, CASE_UPPER=1, CASE_LOWER=2 } CaseConversion;
 *
 *   class ICaseConverter (pure-virtual, one method)
 *     →  ICaseConverter vtable struct (C vtable pattern)
 *        Used only so callers can call CaseConvertString via a pointer;
 *        the concrete type is CaseConverter.
 *
 *   constexpr size_t maxExpansionCaseConversion = 3
 *     →  #define MAX_EXPANSION_CASE_CONVERSION 3
 *
 *   std::string CaseConvertString(const std::string &, CaseConversion)
 *     →  dropped (allocates heap string; no callers in C translation yet)
 *        Use the buffer-based CaseConvertString instead.
 */

#ifndef CASECONVERT_C_H
#define CASECONVERT_C_H

#include <stddef.h>

/* Maximum expansion: one input character may produce up to 3× output bytes. */
#define MAX_EXPANSION_CASE_CONVERSION 3

/* ── CaseConversion ────────────────────────────────────────────────── */
typedef enum {
    CASE_FOLD  = 0,
    CASE_UPPER = 1,
    CASE_LOWER = 2
} CaseConversion;

/* ── ICaseConverter (vtable) ───────────────────────────────────────── */
typedef struct ICaseConverter ICaseConverter;

struct ICaseConverter {
    size_t (*CaseConvertString)(ICaseConverter *self,
                                char *converted, size_t sizeConverted,
                                const char *mixed, size_t lenMixed);
};

/* Retrieve the shared (lazily initialised) converter for a conversion mode. */
ICaseConverter *ConverterFor(CaseConversion conversion);

/* Returns a pointer to a NUL-terminated UTF-8 string containing the
 * converted form of 'character', or NULL when no conversion exists. */
const char *CaseConvert(int character, CaseConversion conversion);

/* Converts 'mixed' (lenMixed bytes) in-place into 'converted' (sizeConverted
 * bytes).  Returns the number of bytes written, or 0 if the buffer is too
 * small.  The caller must allocate at least lenMixed * MAX_EXPANSION_CASE_CONVERSION
 * bytes to be safe. */
size_t CaseConvertString(char *converted, size_t sizeConverted,
                          const char *mixed, size_t lenMixed,
                          CaseConversion conversion);

#endif /* CASECONVERT_C_H */
