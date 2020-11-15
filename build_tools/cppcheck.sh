#!/bin/sh

cppcheck --enable=all --std=posix --quiet \
         --suppressions-list=build_tools/cppcheck.suppressions \
         --rule-file=build_tools/cppcheck.rules \
         --force \
         ./src/
