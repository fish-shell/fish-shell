set -l is_gnu
__fish_supports_version locate; and set is_gnu --is-gnu

__fish_gnu_complete -c locate -s c -l count -d 'Print only the number of matches found' $is_gnu
__fish_gnu_complete -c locate -s i -l ignore-case -d 'Ignore case distinctions' $is_gnu
__fish_gnu_complete -c locate -s 0 -l null -d 'Use ASCII NUL as a separator' $is_gnu
__fish_gnu_complete -c locate -s S -l statistics -d 'Print statistics about databases and exit' $is_gnu

if test -n "$is_gnu"
    complete -c locate -s A -l all -d 'Match all non-option arguments'
    complete -c locate -s b -l basename -d 'Match against the base name of the file'
    complete -c locate -s d -l database -r -d 'Use different DATABASE file[s]'
    complete -c locate -s e -l existing -d 'Match only existing files'
    complete -c locate -s L -l follow -d 'Consider broken symbolic links to be non-existing files'
    complete -c locate -s P -l nofollow -d 'Treat broken symbolic links as if they were existing'
    complete -c locate -s H -l nofollow -d 'Treat broken symbolic links as if they were existing'
    complete -c locate -s l -l limit -r -d 'Limit  the number of matches'
    complete -c locate -s w -l wholename -d 'Match against the whole name of the file'
    complete -c locate -s r -l regex -d 'The pattern is a regular expression'
    complete -c locate -s h -l help -d 'Print a summary of the options and exit'
    complete -c locate -s V -l version -d 'Print the version number and exit'
else
    complete -c locate -s m -d 'Use mmap for searching'
    complete -c locate -s s -d 'Suppress error messages'
    complete -c locate -s l -x -d 'Limit the number of matches'
    complete -c locate -s d -r -d 'Use different DATABASE file'
end
