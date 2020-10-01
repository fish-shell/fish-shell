#
# This function is used internally by the fish command completion code
#

# macOS 10.15 "Catalina" has some major issues.
# The whatis database is non-existent, so apropos tries (and fails) to create it every time,
# which takes about half a second.
#
# Instead, we build a whatis database in the user home directory
# and use grep to query it
#
# A function `__fish_apropos_update` is provided for updating
#
function __fish_apropos -d "Internal command to query the apopos database"
    if test (uname) = Darwin
        set db ~/.whatis.db

        function __fish_apropos_update -d "Updates the apropos database $db on Macos"
            echo "updating apropos / whatis database at $db"
            man --path | tr ":" " " | xargs /usr/libexec/makewhatis -o $db
        end

        [ -f $db ] || __fish_apropos_update

        /usr/bin/grep -i "$argv" $db
    else
        apropos $argv
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
    __fish_apropos $argv 2>/dev/null | awk -v FS=" +- +" '{
		split($1, names, ", ");
		for (name in names)
			if (names[name] ~ /^'"$argv_regex"'.* *\([18]\)/ ) {
				sub( "( |\t)*\\\([18]\\\)", "", names[name] );
				sub( " \\\[.*\\\]", "", names[name] );
				print names[name] "\t" $2;
			}
	}'
end
