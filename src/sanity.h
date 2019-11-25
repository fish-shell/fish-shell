// Prototypes for functions for performing sanity checks on the program state.
#ifndef FISH_SANITY_H
#define FISH_SANITY_H

/// Call this function to tell the program it is not in a sane state.
void sanity_lose();

/// Try and determine if ptr is a valid pointer. If not, loose sanity.
///
/// \param ptr The pointer to validate
/// \param err A description of what the pointer refers to, for use in error messages
/// \param null_ok Whether the pointer is allowed to point to 0
void validate_pointer(const void *ptr, const wchar_t *err, int null_ok);

#endif
