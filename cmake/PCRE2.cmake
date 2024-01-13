# We can either use the system-installed PCRE or our bundled version.
# This is controlled by the cache variable FISH_USE_SYSTEM_PCRE2.
# Here we compute the default value for that variable.
# The actual decision is made by the Rust pcre2-sys crate, which searches for the
# pkg-config file and uses a vendored copy if it is not available.
if ((APPLE) AND (MAC_CODESIGN_ID))
  # On Mac, a codesigned fish will refuse to load a non-codesigned PCRE2
  # (e.g. from Homebrew) so default to bundled PCRE2.
  set(USE_SYS_PCRE2_DEFAULT OFF)
else()
  # Default to using the system PCRE2, if it can be found by the crate
  set(USE_SYS_PCRE2_DEFAULT ON)
endif()

set(FISH_USE_SYSTEM_PCRE2 ${USE_SYS_PCRE2_DEFAULT} CACHE BOOL
  "Try to use PCRE2 from the system, instead of the pcre2-sys version")

if(FISH_USE_SYSTEM_PCRE2)
  message(STATUS "Trying to use PCRE2 from the system")
  set(FISH_PCRE2_BUILDFLAG "")
else()
  message(STATUS "Forcing static build of PCRE2")
  set(FISH_PCRE2_BUILDFLAG "CARGO_FEATURE_STATIC_PCRE2=1")
endif(FISH_USE_SYSTEM_PCRE2)
