# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish %s

ulimit --core-size
#CHECK: {{unlimited|\d+}}
ulimit --core-size 0
ulimit --core-size
#CHECK: 0
