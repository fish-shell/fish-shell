# This file adds commands to manage the FISH-BUILD-VERSION-FILE (hereafter
# FBVF). This file exists in the build directory and is used to populate the
# documentation and also the version string in fish_version.o (printed with
# `echo $version` and also fish --version). The essential idea is that we are
# going to invoke git_version_gen.sh, which will update the
# FISH-BUILD-VERSION-FILE only if it needs to change; this is what makes
# incremental rebuilds fast.
#
# This code is delicate, with the chief subtlety revolving around Ninja. A
# natural and naive approach would tell the generated build system that FBVF is
# a dependency of fish_version.o, and that git_version_gen.sh updates it. Make
# will then invoke the script, check the timestamp on fish_version.o and FBVF,
# see that FBVF is earlier, and then not rebuild fish_version.o. Ninja,
# however, decides what to build up-front and will unconditionally rebuild
# fish_version.o.
#
# To avoid this with Ninja, we want to hook into its 'restat' option which we
# can do through the BYPRODUCTS feature of CMake. See
# https://cmake.org/cmake/help/latest/policy/CMP0058.html
#
# Unfortunately BYPRODUCTS behaves strangely with the Makefile generator: it
# marks FBVF as generated and then CMake itself will `touch` it on every build,
# meaning that using BYPRODUCTS will cause fish_version.o to be rebuilt
# unconditionally with the Makefile generator. Thus we want to use the
# natural-and-naive approach for Makefiles.

# **IMPORTANT** If you touch these build rules, please test both Ninja and
# Makefile generators with both a clean and dirty git tree. Verify that both
# generated build systems rebuild fish when the git tree goes from dirty to
# clean (and vice versa), and verify they do NOT rebuild it when the git tree
# stays the same (incremental builds must be fast).

# Just a handy abbreviation.
SET(FBVF FISH-BUILD-VERSION-FILE)

# TODO: find a cleaner way to do this.
IF (${CMAKE_GENERATOR} STREQUAL Ninja)
  SET(FBVF-OUTPUT fish-build-version-witness.txt)
  SET(CFBVF-BYPRODUCTS ${FBVF})
ELSE(${CMAKE_GENERATOR} STREQUAL Ninja)
  SET(FBVF-OUTPUT ${FBVF})
  SET(CFBVF-BYPRODUCTS)
ENDIF(${CMAKE_GENERATOR} STREQUAL Ninja)

# Set up the version targets
ADD_CUSTOM_TARGET(CHECK-FISH-BUILD-VERSION-FILE
                  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/build_tools/git_version_gen.sh
                  BYPRODUCTS ${CFBVF-BYPRODUCTS})

ADD_CUSTOM_COMMAND(OUTPUT ${FBVF-OUTPUT}
                    DEPENDS CHECK-FISH-BUILD-VERSION-FILE)
