
#
# This is a neat function, stolen from zsh. It allows you to edit the
# value of a variable interactively.
#

function vared -d (N_ "Edit variable value")
	if test (count $argv) = 1
		switch $argv

			case '-h' '--h' '--he' '--hel' '--help'
				help vared

			case '-*'
				printf (_ "%s: Unknown option %s\n") vared $argv

			case '*'
				if test (count $$argv ) -lt 2
					set init ''
					if test $$argv
						set -- init $$argv
					end
					set prompt 'set_color green; echo '$argv'; set_color normal; echo "> "'
					read -p $prompt -c $init tmp

					# If variable already exists, do not add any
					# switches, so we don't change export rules. But
					# if it does not exist, we make the variable
					# global, so that it will not die when this
					# function dies

					if test $$argv
						set -- $argv $tmp
					else
						set -g -- $argv $tmp
					end

				else

					printf (_ '%s: %s is an array variable. Use %svared%s %s[n] to edit the n:th element of %s\n') $argv (set_color $fish_color_command) (set_color $fish_color_normal) vared $argv $argv
				end
		end
	else
		printf (_ '%s: Expected exactly one argument, got %s.\n\nSynopsis:\n\t%svared%s VARIABLE\n') vared (count $argv) (set_color $fish_color_command) (set_color $fish_color_normal)
	end
end

