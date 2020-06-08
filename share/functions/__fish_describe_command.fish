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
    # macOS 15 is Darwin 19, this is an issue up to and including 10.15.3.
    if test "$darwin_version[1]" = 19 -a "$darwin_version[2]" -le 3
        function __fish_describe_command
        end
        # (remember: exit when `source`ing only exits the file, not the shell)
        exit
    end
end

# Perform this check once at startup rather than on each invocation
if not type -q apropos
    function __fish_describe_command
    end
    exit
end

function __fish_describe_command -d "Command used to find descriptions for commands"
    # $argv will be inserted directly into the awk regex, so it must be escaped
    set -l argv_regex (string escape --style=regex "$argv")
    apropos $argv 2>/dev/null | awk -v FS=" +- +" '{
		split($1, names, ", ");
		for (name in names)
			if (names[name] ~ /^'"$argv_regex"'.* *\([18]\)/ ) {
				sub( "( |\t)*\\\([18]\\\)", "", names[name] );
				sub( " \\\[.*\\\]", "", names[name] );
				print names[name] "\t" $2;
			}
	}'
end

