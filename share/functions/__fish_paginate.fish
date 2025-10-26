# localization: skip(private)
function __fish_paginate -d "Paginate the current command using the users default pager"
    set -l cmd (__fish_anypager)
    or return 1

    set -l pipe " &| $cmd"
    if string match -rq -- ' \n\.$' "$(commandline -j; echo .)"
        set pipe "&| $cmd"
    end
    fish_commandline_append $pipe
end
