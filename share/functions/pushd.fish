

function pushd --description "Push directory to stack"
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help pushd
				return 0
		end
	end

	# Comment to avoid set completions
	set -g dirstack (command pwd) $dirstack
	cd $argv[1]
end

