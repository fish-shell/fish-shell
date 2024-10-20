function __fish_guild__complete_warnings
    guild compile -Whelp |
        string replace --filter --regex '^\s+`([a-z\-]+)\'\s+(.+)' '$1\t$2'

    printf '%s\n' 0 2 3
    echo 1\tdefault
end

function __fish_guild__complete_optimizations
    guild compile -Ohelp |
        string replace --filter --regex '^\s+-O(.+)' '$1\nno-$1'

    printf '%s\n' 0 1 3
    echo 2\tdefault
end

set -l command guild

complete -c $command -f

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s v -l version -d 'Show version'

complete -c $command -s L -l load-path -F \
    -d 'Specify the directory to prepend to module load path'

complete -c $command -s o -l output -F \
    -d 'Specify the output file to put bytecode in'

complete -c $command -s x -x \
    -d 'Specify the extension to prepend to extension list'

complete -c $command -s W -l warning \
    -a '(__fish_complete_list , __fish_guild__complete_warnings)' \
    -d 'Specify the warning level for a compilation'

complete -c $command -s O -l optimize \
    -a '(__fish_guild__complete_optimizations)' \
    -d 'Specify the optimization level for a compilation'

for standard in 6 7
    set -l option r$standard"rc"

    complete -c $command -l $option \
        -d "Use $(string upper -- $option) compatible mode"
end

complete -c $command -s f -l from \
    -a 'scheme\tdefault elisp ecmascript' \
    -d 'Specify the language for sources'

complete -c $command -s t -l to \
    -a 'rtl\tdefault' \
    -d 'Specify the language for an output'

complete -c $command -s T -l target \
    -d 'Specify the target for a code'
