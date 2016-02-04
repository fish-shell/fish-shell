function __fish_complete_command --description 'Complete using all available commands'
	set -l ctoken (commandline -ct)
	switch $ctoken
	case '*=*'
		set ctoken (string split "=" -- $ctoken)
		printf '%s\n' $ctoken[1]=(complete -C$ctoken[2])
	case '*'
		complete -C$ctoken
	end
end
