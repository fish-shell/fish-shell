function funced --description 'Edit function definition'
    set -l external ''
    # Default to editing with EDITOR if set
    if set -q EDITOR
        set external $EDITOR
	end

	# Allow overriding the editor with a -e flag
	if test (count $argv) -eq 3
		if contains -- $argv[1] -e --editor
			set external $argv[2]
			set -e argv[2]
			set -e argv[1]
		end
	end
	
	# Let an editor of "fish" mean to use the default editor
	if test "$external" = fish
		set -e external
	end
	
	# Make sure any external editor exists
	if set -q external
		if not type -f "$external" >/dev/null
			set -e external
		end
	end
		
	if test (count $argv) = 1
		switch $argv

			case '-h' '--h' '--he' '--hel' '--help'
				__fish_print_help funced
				return 0

			case '-*'
				printf (_ "%s: Unknown option %s\n") funced $argv
				return 1

			case '*' '.*'
				set -l init ''
				set -l tmp

                if set -q external[1]
                	# Generate a temporary filename.
                	# mktemp has a different interface on different OSes,
                	# or may not exist at all; thus we fake one up from our pid
                    set -l tmpname (printf "$TMPDIR/fish_funced_%d.fish" %self)

                    if functions -q $argv
                        functions $argv > $tmpname
                    else
                        echo function $argv\n\nend > $tmpname
                    end
                    if eval $external $tmpname
                        . $tmpname
                    end
                    set -l stat $status 
                    rm $tmpname >/dev/null
                    return $stat
                end

                set -l IFS
				if functions -q $argv
                    # Shadow IFS here to avoid array splitting in command substitution
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

