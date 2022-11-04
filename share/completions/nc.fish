# SPDX-FileCopyrightText: © 2009 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
# SPDX-FileCopyrightText: © 2022 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# There a several different implementations of netcat.
# Try to figure out which is the current used one
# and load the right set of completions.

set -l flavor
if string match -rq -- '^OpenBSD netcat' (nc -h 2>&1)[1]
    set flavor nc.openbsd
else
    set flavor (basename (realpath (command -v nc)))
end

__fish_complete_netcat nc $flavor
