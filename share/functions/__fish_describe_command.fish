#
# This function is used internally by the fish command completion code
#

function __fish_describe_command -d "Command used to find descriptions for commands"
    # We're going to try to build a regex out of $argv inside awk.
    # Make sure $argv has no special characters.
    # TODO: stop interpolating argv into regex, and remove this hack.
    string match --quiet --regex '^[a-zA-Z0-9_ ]+$' -- "$argv"
    or return
    type -q apropos; or return
    apropos $argv 2>/dev/null | awk -v FS=" +- +" '{
		split($1, names, ", ");
		for (name in names)
			if (names[name] ~ /^'"$argv"'.* *\([18]\)/ ) {
				sub( "( |\t)*\\\([18]\\\)", "", names[name] );
				sub( " \\\[.*\\\]", "", names[name] );
				print names[name] "\t" $2;
			}
	}'
end

