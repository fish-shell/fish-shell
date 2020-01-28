#
# This function is used internally by the fish command completion code
#

# macOS 10.15 "Catalina" has some major issues.
# The whatis database is non-existent, so apropos tries (and fails) to create it every time,
# which takes about half a second.
#
# So we disable this entirely in that case.
if test (uname) = Darwin
    set -l darwin_version (uname -r | string split .)
    # macOS 15 is Darwin 19, this is an issue at least up to 10.15.3.
    # If this is fixed in later versions uncomment the second check.
    if test "$darwin_version[1]" = 19 # -a "$darwin_version[2]" -le 3
        function __fish_describe_command
        end
        # (remember: exit when `source`ing only exits the file, not the shell)
        exit
    end
end

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

