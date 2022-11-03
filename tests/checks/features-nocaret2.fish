# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish --features    'stderr-nocaret' -c 'status test-feature stderr-nocaret; echo nocaret: $status'
# CHECK: nocaret: 0
