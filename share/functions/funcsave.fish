
function funcsave --description "Save the current definition of all specified functions to file"

	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help funcsave
				return 0
		end
	else
		printf (_ "%s: Expected function name\n") funcsave
		__fish_print_help funcsave
		return 1
	end

	set -l res 0

	set -l configdir ~/.config
	if set -q XDG_CONFIG_HOME
		set configdir $XDG_CONFIG_HOME
	end

	for i in $configdir $configdir/fish $configdir/fish/functions
		if not test -d $i
			if not command mkdir $i >/dev/null
				printf (_ "%s: Could not create configuration directory\n") funcsave
				return 1
			end
		end
	end

	for i in $argv
		if functions -q -- $i
			functions -- $i > $configdir/fish/functions/$i.fish
		else
			printf (_ "%s: Unknown function '%s'\n") funcsave $i
			set res 1
		end
	end

	return $res
end

