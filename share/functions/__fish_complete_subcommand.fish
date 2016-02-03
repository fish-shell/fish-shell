function __fish_complete_subcommand  -d "Complete subcommand" --no-scope-shadowing
	set -l skip_next 1
    set -l test
    switch "$argv[1]"
        case '--fcs-skip=*'
            set -l rest
            string replace -a = ' ' -- $argv[1] | read test skip_next
            set -e argv[1]
    end

	set -l res ""
	set -l had_cmd 0
	set -l cmd (commandline -cop) (commandline -ct)

	for i in $cmd

		if test $skip_next -gt 0
            set skip_next (math $skip_next - 1)
			continue
		end

		if test "$had_cmd" = 1
			set res "$res $i"
		else

			if contains -- $i $argv
				set skip_next (math $skip_next + 1)
				continue
			end

			switch $i
				case '-*'
				case '*=*'
				case '*'

					set had_cmd 1
					set res $i
			end
		end
	end

	printf "%s\n" (complete -C$res)

end

