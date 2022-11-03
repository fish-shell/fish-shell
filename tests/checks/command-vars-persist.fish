# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish -c 'set foo bar' -c 'echo $foo'
# CHECK: bar
