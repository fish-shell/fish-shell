function __fish_paginate -d "Paginate the current command using the users default pager"

    set -l cmd less
    if set -q PAGER
        echo $PAGER | read -at cmd
    end

    if test -z (commandline -j | string join '')
        commandline -a $history[1]
    end

    if commandline -j | string match -q -r -v "$cmd *\$"

        commandline -aj " &| $cmd;"
    end

end
