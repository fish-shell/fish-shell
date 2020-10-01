#
# This function is used internally by the fish command completion code
#

# macOS 10.15 "Catalina" has some major issues.
# The whatis database is non-existent, so apropos tries (and fails) to create it every time,
# which takes about half a second.
#
# Instead, we build a whatis database in the user cache directory
# and override the MANPATH using that directory before we run `apropos`
#
# the cache is rebuilt one a day
#
function __fish_apropos
    if test (uname) = Darwin
        set -l cache $HOME/.cache/fish/
        if test -n "$XDG_CACHE_HOME"
            set cache $XDG_CACHE_HOME/fish
        end

        set -l db $cache/whatis
        set -l max_age 86400 # one day
        set -l age $max_age

        if test -f $db
            set age (math (date +%s) - (stat -f %m $db))
        end

        if test $age -ge $max_age
            echo "making cache $age $max_age"
            mkdir -m 700 -p $cache
            man --path | string split : | xargs /usr/libexec/makewhatis -o $db >/dev/null 2>&1
        end
        MANPATH="$cache" apropos $argv
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
