
function funced --description "Edit function definition"
	if test (count $argv) = 1
		switch $argv

			case '-h' '--h' '--he' '--hel' '--help'
				__fish_print_help funced
				return 0

			case '-*'
				printf (_ "%s: Unknown option %s\n") funced $argv
				return 1

			case '*'
				set -l init ''
				set -l tmp
				
				# Shadow IFS here to avoid array splitting in command substitution
				set -l IFS 
				if functions -q $argv
					set init (functions $argv | fish_indent --no-indent)
				else
					set init function $argv\nend
				end

				
				set -l prompt 'printf "%s%s%s> " (set_color green) '$argv' (set_color normal)'
				# Unshadow IFS since the fish_title breaks otherwise
				set -e IFS
				if read -p $prompt -c "$init" -s cmd
					# Shadow IFS _again_ to avoid array splitting in command substitution
					set -l IFS 
					eval (echo -n $cmd | fish_indent)
				end
				return 0
		end
	else
		printf (_ '%s: Expected exactly one argument, got %s.\n\nSynopsis:\n\t%sfunced%s FUNCTION\n') funced (count $argv) (set_color $fish_color_command) (set_color $fish_color_normal)
	end
end

