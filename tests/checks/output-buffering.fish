# RUN: fish=%fish %fish %s

set -g nproc (
    if command -v nproc >/dev/null
        nproc
    else
        sysctl -n hw.logicalcpu
    end
)
function run-concurrently
    for i in (seq (math "10 * min($nproc, 16)"))
        $fish -c $argv[1] &
    end
    wait
end

run-concurrently '
    set t "$(
        /bin/echo -n A
        echo -n B
    )"
    test $t = AB
    or echo $t
'

run-concurrently '
    eval "
        /bin/echo -n C
        echo -n D
    " | read -l t
    test $t = CD
    or echo $t
'

run-concurrently '
    # block/function node output is buffered also
    begin
        /bin/echo -n E
        echo -n F
    end |
        read -l t
    test $t = EF
    or echo $t
'
