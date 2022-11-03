# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish -c 'echo command' -C 'echo init-command'
# CHECK: init-command
# CHECK: command
