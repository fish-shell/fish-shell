# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: echo 'status job-control full; command echo A ; echo B;' | %fish

# Regression test for #6573.

#CHECK: A
#CHECK: B
