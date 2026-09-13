set -l style 'a\t"number all lines"
t\t"number only nonempty lines"
n\t"number no lines"
p\t"number only lines that match regular expression"'

set -l is_gnu
__fish_supports_version nl; and set is_gnu --is-gnu

__fish_gnu_complete -c nl -s b -l body-numbering -d 'use STYLE for numbering body lines' -xa $style $is_gnu
__fish_gnu_complete -c nl -s d -l section-delimiter -x -d 'use for separating logical pages' $is_gnu
__fish_gnu_complete -c nl -s f -l footer-numbering -d 'use STYLE for numbering footer lines' -xa $style $is_gnu
__fish_gnu_complete -c nl -s h -l header-numbering -d 'use STYLE for numbering header lines' -xa $style $is_gnu
__fish_gnu_complete -c nl -s i -l line-increment -x -d 'line number increment at each line' $is_gnu
__fish_gnu_complete -c nl -s l -l join-blank-lines -x -d 'group of NUMBER empty lines counted as one' $is_gnu
__fish_gnu_complete -c nl -s n -l number-format -x -d 'insert line numbers according to FORMAT' $is_gnu
__fish_gnu_complete -c nl -s n -l number-format -xa ln -d 'left justified, no leading zeroes' $is_gnu
__fish_gnu_complete -c nl -s n -l number-format -xa rn -d 'right justified, no leading zeroes' $is_gnu
__fish_gnu_complete -c nl -s n -l number-format -xa rz -d 'right justified, leading zeroes' $is_gnu
__fish_gnu_complete -c nl -s p -l no-renumber -d 'do not reset line numbers at logical pages' $is_gnu
__fish_gnu_complete -c nl -s s -l number-separator -x -d 'add STRING after (possible) line number' $is_gnu
__fish_gnu_complete -c nl -s v -l starting-line-number -x -d 'first line number on each logical page' $is_gnu
__fish_gnu_complete -c nl -s w -l number-width -x -d 'use NUMBER columns for line numbers' $is_gnu

if test -n "$is_gnu"
    complete -c nl -l help -d 'display this help and exit'
    complete -c nl -l version -d 'output version information and exit'
end
