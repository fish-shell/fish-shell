# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish -c '%fish -c false; echo RC: $status'
# CHECK: RC: 1
