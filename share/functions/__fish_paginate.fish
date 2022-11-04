# SPDX-FileCopyrightText: © 2007 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_paginate -d "Paginate the current command using the users default pager"
    set -l cmd less
    if set -q PAGER
        echo $PAGER | read -at cmd
    end

    fish_commandline_append " &| $cmd"
end
