#
# Completions for sed
#

# Test if we are using GNU sed

set -l is_gnu 
sed --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

# Shared ls switches

__fish_gnu_complete -c sed -s n -l quiet -d (N_ "Silent mode") $is_gnu 
__fish_gnu_complete -c sed -s e -l expression -x -d (N_ "Evaluate expression") $is_gnu
__fish_gnu_complete -c sed -s f -l file -r -d (N_ "Evalute file") $is_gnu
__fish_gnu_complete -c sed -s i -l in-place -d (N_ "Edit files in place") $is_gnu

if test -n "$is_gnu"

	# GNU specific features

	complete -c sed -l silent -d (N_ "Silent mode")
	complete -c sed -s l -l line-length -x -d (N_ "Specify line-length")
	complete -c sed -l posix -d (N_ "Disable all GNU extensions")
	complete -c sed -s r -l regexp-extended -d (N_ "Use extended regexp")
	complete -c sed -s s -l separate -d (N_ "Consider files as separate")
	complete -c sed -s u -l unbuffered -d (N_ "Use minimal IO buffers")
	complete -c sed -l help -d (N_ "Display help and exit")
	complete -c sed -s V -l version -d (N_ "Display version and exit")

else

	# If not a GNU system, assume we have standard BSD ls features instead

	complete -c sed -s E -d (N_ "Use extended regexp")
	complete -c sed -s a -d (N_ "Delay opening files until a command containing the related 'w' function is applied")
	complete -c sed -s l -d (N_ "Use line buffering")

end
