# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s
# Tests for the "builtin" builtin/keyword.
builtin -q string; and echo String exists
#CHECK: String exists
builtin -q; and echo None exists
builtin -q string echo banana; and echo Some of these exist
#CHECK: Some of these exist
builtin -nq string
#CHECKERR: builtin: invalid option combination, --query and --names are mutually exclusive
echo $status
#CHECK: 2
exit 0
