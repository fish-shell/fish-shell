function __fish_complete_lpr_option --description 'Complete lpr option'
	set -l optstr (commandline -t)
	switch $optstr
	case '*=*'
		set -l IFS =
		echo $optstr | read -l opt val
		set -l descr
		# Some seds (e.g. on Mac OS X), don't support \n in the RHS
		# Use a literal newline instead
		# http://sed.sourceforge.net/sedfaq4.html#s4.1
		for l in (lpoptions -l ^ /dev/null | grep $opt | sed 's+\(.*\)/\(.*\):\s*\(.*\)$+\2 \3+; s/ /\\
/g;')
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
