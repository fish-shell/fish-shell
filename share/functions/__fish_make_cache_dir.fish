# SPDX-FileCopyrightText: © 2020 fish-shell contributors
#
# SPDX-License-Identifier: GPL-2.0-only

function __fish_make_cache_dir --description "Create and return XDG_CACHE_HOME"
    set -l xdg_cache_home $XDG_CACHE_HOME
    if test -z "$xdg_cache_home"; or string match -qv '/*' -- $xdg_cache_home; or set -q xdg_cache_home[2]
        set xdg_cache_home $HOME/.cache
    end
    mkdir -m 700 -p $xdg_cache_home
    and echo $xdg_cache_home
end
