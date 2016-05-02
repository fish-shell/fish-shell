// Library for pooling common strings.
#ifndef FISH_INTERN_H
#define FISH_INTERN_H

/// Return an identical copy of the specified string from a pool of unique strings. If the string
/// was not in the pool, add a copy.
///
/// \param in the string to return an interned copy of.
const wchar_t *intern(const wchar_t *in);

/// Insert the specified string literal into the pool of unique strings. The string will not first
/// be copied, and it will not be free'd on exit.
///
/// \param in the string to add to the interned pool
const wchar_t *intern_static(const wchar_t *in);

#endif
