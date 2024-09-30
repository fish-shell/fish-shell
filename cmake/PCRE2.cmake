set(FISH_USE_SYSTEM_PCRE2 ON CACHE BOOL
  "Try to use PCRE2 from the system, instead of the pcre2-sys version")

if(FISH_USE_SYSTEM_PCRE2)
  message(STATUS "Trying to use PCRE2 from the system")
  set(FISH_PCRE2_BUILDFLAG "")
else()
  message(STATUS "Forcing static build of PCRE2")
  set(ENV{PCRE2_SYS_STATIC} "1")
endif(FISH_USE_SYSTEM_PCRE2)
