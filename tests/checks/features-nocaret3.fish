# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish --features 'no-stderr-nocaret' -c 'echo -n careton:; echo ^/dev/null'
# CHECK: careton:^/dev/null
