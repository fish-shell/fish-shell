#!/bin/sh

# SPDX-FileCopyrightText: © 2015 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

cppcheck --std=c++11 --quiet \
         --suppressions-list=build_tools/cppcheck.suppressions --inline-suppr \
         --rule-file=build_tools/cppcheck.rules \
         --force \
         ${@:---enable=all ./src/}
