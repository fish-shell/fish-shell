function __fish_complete_command --description 'Complete using all available commands'
	set -l ctoken (commandline -ct)
	switch $ctoken
	case '*=*'
		set ctoken (echo $ctoken  | sed 's/=/\n/')
		printf '%s\n' $ctoken[1]=(complete -C$ctoken[2])
	case '*'
		complete -C$ctoken
	end
end
