# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish -C 'echo init-command' -C 'echo 2nd init-command'
# CHECK: init-command
# CHECK: 2nd init-command
