function __fish_guile__complete_srfis
    printf '%s\n' 0\t'cond-expand' \
        1\t'List library' \
        2\t'and-let*' \
        4\t'Homogeneous numeric vector datatypes' \
        6\t'Basic String Ports' \
        8\t'receive' \
        9\t'define-record-type' \
        10\t'Hash-Comma Reader Extension' \
        11\t'let-values' \
        13\t'String Library' \
        14\t'Character-set Library' \
        16\t'case-lambda' \
        17\t'Generalized set!' \
        18\t'Multithreading support' \
        19\t'Time/Date Library' \
        23\t'Error Reporting' \
        26\t'specializing parameters' \
        27\t'Sources of Random Bits' \
        28\t'Basic Format Strings' \
        30\t'Nested Multi-line Comments' \
        31\t'A special form ‘rec’ for recursive evaluation' \
        34\t'Exception handling for programs' \
        35\t'Conditions' \
        37\t'args-fold' \
        38\t'External Representation for Data With Shared Structure' \
        39\t'Parameters' \
        41\t'Streams' \
        42\t'Eager Comprehensions' \
        43\t'Vector Library' \
        45\t'Primitives for Expressing Iterative Lazy Algorithms' \
        46\t'Basic syntax-rules Extensions' \
        55\t'Requiring Features' \
        60\t'Integers as Bits' \
        61\t'A more general cond clause' \
        62\t'S-expression comments' \
        64\t'A Scheme API for test suites' \
        67\t'Compare procedures' \
        69\t'Basic hash tables' \
        71\t'Extended let-syntax for multiple values' \
        87\t'in case clauses' \
        88\t'Keyword Objects' \
        98\t'Accessing environment variables' \
        105\t'Curly-infix expressions' \
        111\t'Boxes' \
        119\t'Wisp: simpler indentation-sensitive Scheme'
end

function __fish_guile__complete_function_names
    set -l path (commandline -poc |
        string match --regex '.*\\.scm$' |
        sed -n 1p)

    test -e "$path" && begin
        cat $path |
            string match --all --groups-only --regex '\(\s*define\s+\(\s*(\w+)'
    end
end

set -l command guile
complete -c $command -f

complete -c $command -s h -l help -d 'Show help'
complete -c $command -s v -l version -d 'Show version'
complete -c $command -s s -F -r -d 'Specify the script to run'
complete -c $command -s c -x -d 'Specify the code to run'

complete -c $command -s L -F -r \
    -d 'Specify the directory to prepend to module load path'

complete -c $command -s C -F -r \
    -d 'Specify the directory to prepend to module load path for compiled files'

complete -c $command -s x -x \
    -a '.scm\tdefault' \
    -d 'Specify the extension to prepend to extension list'

complete -c $command -s l -F -r -d 'Specify the script to load'

complete -c $command -s e -x \
    -a '(__fish_guile__complete_function_names)' \
    -d 'Specify the entry point of a script'

complete -c $command -o ds \
    -d 'Treat the last -s option as if it occurred at this point'

complete -c $command -l use-srfi \
    -a '(__fish_complete_list , __fish_guile__complete_srfis)' \
    -d 'Specify the SRFI modules to load'

for standard in 6 7
    set -l option r$standard"rc"

    complete -c $command -l $option \
        -d "Use $(string upper -- $option) compatible mode"
end

complete -c $command -l debug -d 'Use debug mode'
complete -c $command -l no-debug -d "Don't use debug mode"
complete -c $command -s q -d "Don't load .guile file"

complete -c $command -l listen \
    -a '37146\tdefault' \
    -d 'Specify the port to list to'

complete -c $command -l auto-compile -d 'Compile scripts automatically'

complete -c $command -l fresh-auto-compile \
    -d 'Compile scripts automatically forcefully'

complete -c $command -l no-auto-compile -d "Don't compile scripts automatically"

complete -c $command -l language \
    -a 'scheme\tdefault elisp ecmascript' \
    -d 'Specify the language for sources'
