#
# Completions for the 'set' builtin
#

#
# We depend on set_color completions for color definitions
#

complete -y set_color

#
# All locale variables used by set completions
#

set -g __fish_locale_vars LANG LC_ALL LC_COLLATE LC_CTYPE LC_MESSAGES LC_MONETARY LC_NUMERIC LC_TIME

#
# Various helper functions
#

function __fish_set_is_color -d 'Test if We are specifying a color value for the prompt'
	set cmd (commandline -poc)
	set -e cmd[1]
	for i in $cmd
		switch $i

			case 'fish_color_*' 'fish_pager_color_*'
				return 0

			case '-*'
				 
			case '*'
				 return 1
		end
	end	
	return 1
end

function __fish_set_is_locale -d 'Test if We are specifying a locale value for the prompt'
	set cmd (commandline -poc)
	set -e cmd[1]
	for i in $cmd
		switch $i

			case $__fish_locale_vars
				return 0

			case '-*'
				 
			case '*'
				 return 1
		end
	end	
	return 1
end

#
# Completions
#

complete -c set -s e -l erase -d (_ "Erase variable")
complete -c set -s x -l export -d (_ "Export variable to subprocess")
complete -c set -s u -l unexport -d (_ "Do not export variable to subprocess")
complete -c set -s g -l global -d (_ "Make variable scope global")
complete -c set -s l -l local -d (_ "Make variable scope local")
complete -c set -s U -l universal -d (_ "Make variable scope universal, i.e. share variable with all the users fish processes on this computer")
complete -c set -s q -l query -d (_ "Test if variable is defined")
complete -c set -s h -l help -d (_ "Display help and exit")

# Complete using preexisting variables
complete -c set -n '__fish_is_first_token' -x -a "(set|sed -e 's/ /\tVariable: /')"

# Color completions
complete -c set -n '__fish_set_is_color' -x -a '(set_color --print-colors)' -d (_ Color)

# Locale completions
complete -c set -n '__fish_is_first_token' -x -a '$__fish_locale_vars' -d 'Locale variable'
complete -c set -n '__fish_set_is_locale' -x -a '(locale -a)' -d (_ Locale)
