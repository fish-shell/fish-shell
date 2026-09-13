set -l is_gnu
__fish_supports_version tee; and set is_gnu --is-gnu

__fish_gnu_complete -c tee -s a -l append -d 'append to the given FILEs, do not overwrite' $is_gnu
__fish_gnu_complete -c tee -s i -l ignore-interrupts -d 'ignore interrupt signals' $is_gnu

if test -n "$is_gnu"
    complete -c tee -l help -d 'display this help and exit'
    complete -c tee -l version -d 'output version information and exit'
end
