
function save_function -d (N_ "Save the current definition of all specified functions to file")

	set -l res 0

	set -l configdir ~/.config
	if set -q XDG_CONFIG_HOME
		set configdir $XDG_CONFIG_HOME
	end

	for i in $configdir $configdir/fish $configdir/fish/functions
		if not test -d 
			if not builtin mkdir $configdir >/dev/null
				printf (_ "%s: Could not create configuration directory\n") save_function
				return 1
			end
		end
	end

	for i in $argv
		if functions -q $i
			functions $i > $configdir/fish/functions/$i.fish
			functions -e $i
		else
			printf (_ "%s: Unknown function '%s'\n") save_function $i
			set res 1
		end
	end

	return $res
end

