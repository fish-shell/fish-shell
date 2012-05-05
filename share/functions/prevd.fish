
function prevd --description "Move back in the directory history"

	if count $argv >/dev/null
		switch $argv[1]
			case -h --h --he --hel --help
				__fish_print_help prevd
				return 0
		end
	end

	# Parse arguments
	set -l show_hist 0
	set -l times 1
    if count $argv > /dev/null
        for i in (seq (count $argv))
            switch $argv[$i]
                case '-l' --l --li --lis --list
                    set show_hist 1
                    continue
                case '-*'
                    printf (_ "%s: Unknown option %s\n" ) prevd $argv[$i]
                    return 1
                case '*'
                    if test $argv[$i] -ge 0 ^/dev/null
                        set times $argv[$i]
                    else
                        printf (_ "The number of positions to skip must be a non-negative integer\n")
                        return 1
                    end
                    continue
            end
        end
	end

	# Traverse history
	set -l code 1
    if count $times > /dev/null
        for i in (seq $times)
            # Try one step backward
            if __fish_move_last dirprev dirnext;
                # We consider it a success if we were able to do at least 1 step
                # (low expectations are the key to happiness ;)
                set code 0
            else
                break
            end
        end
    end

	# Show history if needed
	if test $show_hist = 1
		dirh
	end

	# Set direction for 'cd -'
	if test $code = 0 ^/dev/null
		set -g __fish_cd_direction next
	end

	# All done
	return $code
end
