#
# Completions for sed
#

# Test if we are using GNU sed

set -l is_gnu
sed --version >/dev/null 2>/dev/null; and set is_gnu --is-gnu

# Shared ls switches

__fish_gnu_complete -c sed -s n -l quiet -d "Silent mode" $is_gnu
__fish_gnu_complete -c sed -s e -l expression -x -d "Evaluate expression" $is_gnu
__fish_gnu_complete -c sed -s f -l file -r -d "Evalute file" $is_gnu
__fish_gnu_complete -c sed -s i -l in-place -d "Edit files in place" $is_gnu

if test -n "$is_gnu"

    # GNU specific features

    complete -c sed -l silent -d "Silent mode"
    complete -c sed -s l -l line-length -x -d "Specify line-length"
    complete -c sed -l posix -d "Disable all GNU extensions"
    complete -c sed -s r -l regexp-extended -d "Use extended regexp"
    complete -c sed -s s -l separate -d "Consider files as separate"
    complete -c sed -s u -l unbuffered -d "Use minimal IO buffers"
    complete -c sed -l help -d "Display help and exit"
    complete -c sed -s V -l version -d "Display version and exit"

else

    # If not a GNU system, assume we have standard BSD ls features instead

    complete -c sed -s E -d "Use extended regexp"
    complete -c sed -s a -d "Delay opening files until a command containing the related 'w' function is applied"
    complete -c sed -s l -d "Use line buffering"

end
