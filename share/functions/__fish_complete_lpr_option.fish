function __fish_complete_lpr_option --description 'Complete lpr option'
	set -l optstr (commandline -t)
	switch $optstr
	case '*=*'
		set -l IFS =
		echo $optstr | read -l opt val
		set -l descr
		for l in (lpoptions -l ^ /dev/null | grep $opt | sed 's+\(.*\)/\(.*\):\s*\(.*\)$+\2 \3+; s/ /\n/g;')
			if not set -q descr[1]
				set descr $l
				continue
			end
			set -l default ''
			if test (expr substr $l 1 1) = '*'
				set default 'Default '
				set l (echo $l | sed 's/\*//')
			end
			echo $opt=$l\t$default$descr
		end
	case '*'
		lpoptions -l ^ /dev/null | sed 's+\(.*\)/\(.*\):.*$+\1=\t\2+'
	end


end
