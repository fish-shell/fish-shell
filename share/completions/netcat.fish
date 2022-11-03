# SPDX-FileCopyrightText: © 2009 Axel Liljencrantz
# SPDX-FileCopyrightText: © 2009 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

# There a several different implementations of netcat.
# Try to figure out which is the current used one
# and load the right set of completions.

set -l flavor
if string match -rq -- '^OpenBSD netcat' (netcat -h 2>&1)[1]
    set flavor nc.openbsd
else
    set flavor (basename (realpath (command -v netcat)))
end

__fish_complete_netcat netcat $flavor
