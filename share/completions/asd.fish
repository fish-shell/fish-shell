# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

complete -f asd
complete -c asd -n "test (__fish_number_of_cmd_args_wo_opts) = 1" \
    -a "sync unsync resync"
