set -l is_gnu
__fish_supports_version unexpand; and set is_gnu --is-gnu

__fish_gnu_complete -c unexpand -s a -l all -d 'convert all blanks, instead of just initial blanks' $is_gnu

if test -n "$is_gnu"
    complete -c unexpand -l first-only -d 'convert only leading sequences of blanks (overrides -a)'
    complete -c unexpand -s t -l tabs -x -d 'have tabs N characters apart instead of 8 (enables -a)'
    complete -c unexpand -s t -l tabs -x -d 'use comma separated LIST of tab positions (enables -a)'
    complete -c unexpand -l help -d 'display this help and exit'
    complete -c unexpand -l version -d 'output version information and exit'
else
    complete -c unexpand -s t -x -d 'have tabs NUMBER characters apart, not 8'
end
