
function isatty -d "Tests if a file descriptor is a tty"
	set -l fd 0
	if count $argv >/dev/null 
		switch $argv[1]

			case -h --h --he --hel --help
				help isatty

			case stdin
				set fd 0

			case stdout
				set fd 1

			case stderr
				set fd 2

			case '*'
				set fd $argv[1]
			
		end
	end

	eval "tty 0>&$fd >/dev/null"; and return 0

	return 1
end
