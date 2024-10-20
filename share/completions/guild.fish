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

set -l compile_condition '__fish_seen_subcommand_from compile'
complete -c $command -a 'compile\tCompile scripts' -n "not $compile_condition"

complete -c $command -s h -l help -d 'Show help' -n $compile_condition
complete -c $command -l version -d 'Show version' -n $compile_condition

complete -c $command -s L -l load-path -F \
    -d 'Specify the directory to prepend to module load path' \
    -n $compile_condition

complete -c $command -s o -l output -F \
    -d 'Specify the output file to put bytecode in' \
        -n $compile_condition

complete -c $command -s x -x \
    -d 'Specify the extension to prepend to extension list' \
        -n $compile_condition

complete -c $command -s W -l warning \
    -a '(__fish_complete_list , __fish_guild__complete_warnings)' \
    -d 'Specify the warning level for a compilation' \
        -n $compile_condition

complete -c $command -s O -l optimize \
    -a '(__fish_guild__complete_optimizations)' \
    -d 'Specify the optimization level for a compilation' \
        -n $compile_condition

for standard in 6 7
    set -l option r$standard"rc"

    complete -c $command -l $option \
        -d "Use $(string upper -- $option) compatible mode" \
            -n $compile_condition
end

complete -c $command -s f -l from \
    -a 'scheme\tdefault elisp ecmascript' \
    -d 'Specify the language for sources' \
        -n $compile_condition

complete -c $command -s t -l to \
    -a 'rtl\tdefault' \
    -d 'Specify the language for an output' \
        -n $compile_condition

complete -c $command -s T -l target \
    -d 'Specify the target for a code' \
        -n $compile_condition
