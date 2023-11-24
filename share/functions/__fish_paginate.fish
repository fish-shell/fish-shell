function __fish_paginate -d "Paginate the current command using the users default pager"
    set -l cmd (__fish_anypager)
    or return 1

    fish_commandline_append " &| $cmd"
end
