# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish --features 'qmark-noglob' -C 'string match --quiet "??" ab ; echo "qmarkoff: $status"'
# CHECK: qmarkoff: 1
