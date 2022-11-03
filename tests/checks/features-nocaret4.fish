# SPDX-FileCopyrightText: © 2019 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

#RUN: %fish --features '   stderr-nocaret' -c 'echo -n "caretoff: "; echo ^/dev/null'
# CHECK: caretoff: ^/dev/null
