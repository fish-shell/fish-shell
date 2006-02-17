function __fish_complete_subcommand  -d "Complete subcommand"
	set -l res ""
	set -l had_cmd 0
	set -l cmd (commandline -cop) (commandline -ct)
	set -l skip_next 1

	for i in $cmd

		if test "$skip_next" = 1
			set skip_next 0
			continue
		end

		if test "$had_cmd" = 1
			set res "$res $i"
		else

			if contains -- $i $argv
				set skip_next 1
				continue
			end

			switch $i
				case '-*'
					 
				case '*'
					set had_cmd 1
					set res $i
			end
		end
	end
	
	printf "%s\n" (commandline -ct)(complete -C $res)

end

