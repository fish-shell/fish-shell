function __fish_complete_command --description 'Complete using all available commands'
	set -l ctoken (commandline -ct)
	switch $ctoken
	case '*=*'
        # Some seds (e.g. on Mac OS X), don't support \n in the RHS
        # Use a literal newline instead
        # http://sed.sourceforge.net/sedfaq4.html#s4.1
		set ctoken (echo $ctoken  | sed 's/=/\\
/')
		printf '%s\n' $ctoken[1]=(complete -C$ctoken[2])
	case '*'
		complete -C$ctoken
	end
end
