# There a several different implementations of netcat.
# Try to figure out which is the current used one
# and load the right set of completions.

set -l flavor
if string match -rq -- '^OpenBSD netcat' (netcat -h 2>&1)[1]
    set flavor nc.openbsd
else
    set flavor (command -s netcat | path resolve | path basename)
end

__fish_complete_netcat netcat $flavor
