#
# Completions for sed
#

# Test if we are using GNU sed

set -l is_gnu
sed --version >/dev/null ^/dev/null; and set is_gnu --is-gnu

# Shared ls switches

__fish_gnu_complete -c sed -s n -l quiet --description "Silent mode" $is_gnu
__fish_gnu_complete -c sed -s e -l expression -x --description "Evaluate expression" $is_gnu
__fish_gnu_complete -c sed -s f -l file -r --description "Evalute file" $is_gnu
__fish_gnu_complete -c sed -s i -l in-place --description "Edit files in place" $is_gnu

if test -n "$is_gnu"

	# GNU specific features

	complete -c sed -l silent --description "Silent mode"
	complete -c sed -s l -l line-length -x --description "Specify line-length"
	complete -c sed -l posix --description "Disable all GNU extensions"
	complete -c sed -s r -l regexp-extended --description "Use extended regexp"
	complete -c sed -s s -l separate --description "Consider files as separate"
	complete -c sed -s u -l unbuffered --description "Use minimal IO buffers"
	complete -c sed -l help --description "Display help and exit"
	complete -c sed -s V -l version --description "Display version and exit"

else

	# If not a GNU system, assume we have standard BSD ls features instead

	complete -c sed -s E --description "Use extended regexp"
	complete -c sed -s a --description "Delay opening files until a command containing the related 'w' function is applied"
	complete -c sed -s l --description "Use line buffering"

end
