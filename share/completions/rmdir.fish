#Completions for rmdir
complete -x -c rmdir -a "(__fish_complete_directories (commandline -ct))"
set -l is_gnu
__fish_supports_version rmdir; and set is_gnu --is-gnu

__fish_gnu_complete -c rmdir -s p -l parents -d "Remove each component of path" $is_gnu
__fish_gnu_complete -c rmdir -s v -l verbose -d "Verbose mode" $is_gnu

if test -n "$is_gnu"
    complete -c rmdir -l ignore-fail-on-non-empty -d "Ignore errors from non-empty directories"
    complete -c rmdir -l help -d "Display help and exit"
    complete -c rmdir -l version -d "Display version and exit"
end
