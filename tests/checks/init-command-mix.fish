# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish -C 'echo init-command' -c 'echo command'
# CHECK: init-command
# CHECK: command
