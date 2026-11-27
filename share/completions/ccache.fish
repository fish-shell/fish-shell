# Based on version 4.14.3.

function __fish_ccache_wraps_compiler
    set -l first (commandline -xpc)[2]
    set -q first[1]
    # Any `-*` option means ccache will ignore the compiler.
    and not string match -q -- '-*' $first
end

complete -c ccache -f

# No need to provide `--` args to `__fish_complete_subcommand`,
# because ccache only runs compiler if no ccache flags provided.
# E.g. `ccache --dir . gcc` simply ignores gcc.
complete -c ccache -n '__fish_is_first_arg; or __fish_ccache_wraps_compiler' -a "(__fish_complete_subcommand)"

complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s c -l cleanup -d "delete not recently used files and recalculate size counters"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s C -l clear -d "clear the cache completely (except configuration)"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l config-path -rF -d "operate on configuration file PATH instead of the default"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s d -l dir -rfa "(__fish_complete_directories)" -d "operate on cache directory PATH instead of the default"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l dry-run -d "do not perform any write operations"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l evict-namespace -rf -d "remove files created in namespace NAMESPACE"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l evict-older-than -rf -d "remove files used less recently than AGE"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s F -l max-files -rf -d "set maximum number of files in cache to NUM"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s M -l max-size -rf -d "set maximum size of cache to SIZE"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s X -l recompress -rf -d "recompress the cache to level LEVEL"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s o -l set-config -rf -d "set configuration option KEY to value VALUE"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s x -l show-compression -d "show compression statistics"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s p -l show-config -d "show current configuration options"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l show-log-stats -d "print statistics counters from the stats log"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s s -l show-stats -d "show summary of configuration and statistics counters"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l threads -rf -d "use up to THREADS threads for threaded operations"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s v -l verbose -d "increase verbosity"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s z -l zero-stats -d "zero statistics counters"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s h -l help -d "print help text"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s V -l version -d "print version and copyright information"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l trim-dir -rfa "(__fish_complete_directories)" -d "trim directory PATH to --trim-max-size"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l trim-max-size -rf -d "specify the maximum size for --trim-dir"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l trim-method -rfa "atime mtime" -d "specify the method for --trim-dir"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l trim-recompress -rf -d "recompress to level LEVEL"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l checksum-file -rF -d "print the checksum (128 bit XXH3) of the file at PATH"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l extract-result -rF -d "extract result file at PATH to the current directory"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l format -rfa "tab json" -d "specify format for --print-log-stats and --print-stats"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -s k -l get-config -rf -d "print the value of configuration key KEY"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l hash-file -rF -d "print the hash (160 bit BLAKE3) of the file at PATH"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l inspect -rF -d "print result/manifest file at PATH"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l stop-storage-helpers -d "stop running storage helper(s), if any"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l print-log-stats -d "print stats log counters in machine-parsable format"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l print-stats -d "print stats counters in machine-parsable format"
complete -c ccache -n 'not __fish_ccache_wraps_compiler' -l print-version -d "print version only"
