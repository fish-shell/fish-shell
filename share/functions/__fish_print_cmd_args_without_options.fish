# SPDX-FileCopyrightText: © 2013 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_print_cmd_args_without_options
    __fish_print_cmd_args | string match -re '^[^-]'
end
