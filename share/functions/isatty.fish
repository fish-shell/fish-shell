
function isatty -d "Tests if a file descriptor is a tty"
	set -l fd 0
	if count $argv >/dev/null
		switch $argv[1]

			case -h --h --he --hel --help
				__fish_print_help isatty
				return 0

			case stdin ''
				set fd 0

			case stdout
				set fd 1

			case stderr
				set fd 2

			case '*'
				set fd $argv[1]

		end
	end

	# Use `command test` because `builtin test` doesn't open the regular fd's.
	# See https://github.com/fish-shell/fish-shell/issues/1228
	command test -t "$fd"

end
