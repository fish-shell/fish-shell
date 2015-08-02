
function popd --description "Pop dir from stack"
	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help popd
				return 0
		end
	end

	if test $dirstack[1]
		cd $dirstack[1]
	else
		printf (_ "%s: Directory stack is empty...\n") popd
		return 1
	end

	set -e dirstack[1]

end
