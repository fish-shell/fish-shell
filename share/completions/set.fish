#
# Completions for the 'set' builtin
#

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

# Regular switches, set only accepts these before the variable name,
# so we need to test using __fish_is_first_token

complete -c set -n '__fish_is_first_token' -s e -l erase -d (N_ "Erase variable")
complete -c set -n '__fish_is_first_token' -s x -l export -d (N_ "Export variable to subprocess")
complete -c set -n '__fish_is_first_token' -s u -l unexport -d (N_ "Do not export variable to subprocess")
complete -c set -n '__fish_is_first_token' -s g -l global -d (N_ "Make variable scope global")
complete -c set -n '__fish_is_first_token' -s l -l local -d (N_ "Make variable scope local")
complete -c set -n '__fish_is_first_token' -s U -l universal -d (N_ "Make variable scope universal, i.e. share variable with all the users fish processes on this computer")
complete -c set -n '__fish_is_first_token' -s q -l query -d (N_ "Test if variable is defined")
complete -c set -n '__fish_is_first_token' -s h -l help -d (N_ "Display help and exit")

# Complete using preexisting variable names
complete -c set -n '__fish_is_first_token' -x -a "(set|sed -e 's/ /'\t'Variable: /')"

# Color completions
complete -c set -n '__fish_set_is_color' -x -a '(set_color --print-colors)' -d (N_ Color)
complete -c set -n '__fish_set_is_color' -s b -l background -x -a '(set_color --print-colors)' -d (N_ "Change background color")
complete -c set -n '__fish_set_is_color' -s o -l bold -d (N_ 'Make font bold')

# Locale completions
complete -c set -n '__fish_is_first_token' -x -a '$__fish_locale_vars' -d 'Locale variable'
complete -c set -n '__fish_set_is_locale' -x -a '(locale -a)' -d (_ Locale)
