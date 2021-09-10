#!/bin/sh

cppcheck --std=c++11 --quiet \
         --suppressions-list=build_tools/cppcheck.suppressions --inline-suppr \
         --rule-file=build_tools/cppcheck.rules \
         --force \
         ${@:---enable=all ./src/}
