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
				continue

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

complete -c set -n '__fish_is_first_token' -s e -l erase -d "Erase variable"
complete -c set -n '__fish_is_first_token' -s x -l export -d "Export variable to subprocess"
complete -c set -n '__fish_is_first_token' -s u -l unexport -d "Do not export variable to subprocess"
complete -c set -n '__fish_is_first_token' -s g -l global -d "Make variable scope global"
complete -c set -n '__fish_is_first_token' -s l -l local -d "Make variable scope local"
complete -c set -n '__fish_is_first_token' -s U -l universal -d "Share variable persistently across sessions"
complete -c set -n '__fish_is_first_token' -s q -l query -d "Test if variable is defined"
complete -c set -n '__fish_is_first_token' -s h -l help -d "Display help and exit"
complete -c set -n '__fish_is_first_token' -s n -l names -d "List the names of the variables, but not their value"
complete -c set -n '__fish_is_first_token' -s a -l append -d "Append value to a list"
complete -c set -n '__fish_is_first_token' -s p -l prepend -d "Prepend value to a list"
complete -c set -n '__fish_is_first_token' -s S -l show -d "Show variable"

#TODO: add CPP code to generate list of read-only variables and exclude them from the following completions

# Complete using preexisting variable names
complete -c set -n '__fish_is_first_token; and not __fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s g -s U -l local -l global -l universal' -x -a "(set -l | string match -rv '^__' | string replace ' ' \t'Local Variable: ')"
complete -c set -n '__fish_is_first_token; and not __fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s g -s U -l local -l global -l universal' -x -a "(set -g | string match -rv '^__' | string replace ' ' \t'Global Variable: ')"
complete -c set -n '__fish_is_first_token; and not __fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s g -s U -l local -l global -l universal' -x -a "(set -U | string match -rv '^__' | string replace ' ' \t'Universal Variable: ')"
# Complete scope-specific variables
complete -c set -n '__fish_is_first_token; and __fish_seen_argument -s l -l local' -x -a "(set -l | string match -rv '^__' | string replace ' ' \t'Local Variable: ')"
complete -c set -n '__fish_is_first_token; and __fish_seen_argument -s g -l global' -x -a "(set -g | string match -rv '^__' | string replace ' ' \t'Global Variable: ')"
complete -c set -n '__fish_is_first_token; and __fish_seen_argument -s U -l universal' -x -a "(set -U | string match -rv '^__' | string replace ' ' \t'Universal Variable: ')"

# Complete using preexisting variable names for `set --erase`
complete -c set -n '__fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s U -s g -l local -l global -l Universal' -f -a "(set -g | string match -rv '^_|^fish_' | string replace ' ' \tGlobal\ Variable:\ )"
complete -c set -n '__fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s U -s g -l local -l global -l Universal' -f -a "(set -l | string match -rv '^_|^fish_' | string replace ' ' \tLocal\ Variable:\ )"
complete -c set -n '__fish_seen_argument -s e -l erase; and not __fish_seen_argument -s l -s U -s g -l local -l global -l Universal' -f -a "(set -U | string match -rv '^_|^fish_' | string replace ' ' \tUniversal\ Variable:\ )"
# Complete scope-specific variables for `set --erase`
complete -c set -n '__fish_seen_argument -s e -l erase; and __fish_seen_argument -s g -l global' -f -a "(set -g | string match -rv '^_|^fish_' | string replace ' ' \t'Global Variable: ')"
complete -c set -n '__fish_seen_argument -s e -l erase; and __fish_seen_argument -s U -l universal' -f -a "(set -u | string match -rv '^_|^fish_' | string replace ' ' \t'Universal Variable: ')"
complete -c set -n '__fish_seen_argument -s e -l erase; and __fish_seen_argument -s l -l local' -f -a "(set -l | string match -rv '^_|^fish_' | string replace ' ' \t'Local Variable: ')"

# Color completions
complete -c set -n '__fish_set_is_color' -x -a '(set_color --print-colors)' -d Color
complete -c set -n '__fish_set_is_color' -s b -l background -x -a '(set_color --print-colors)' -d "Change background color"
complete -c set -n '__fish_set_is_color' -s o -l bold -d 'Make font bold'

# Locale completions
complete -c set -n '__fish_set_is_locale; and not __fish_seen_argument -s e -l erase' -x -a '(locale -a)' -d (_ "Locale")
complete -c set -s L -l long -d 'Do not truncate long lines'
