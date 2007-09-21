
function __fish_complete_command --description "Complete using all available commands"
	printf "%s\n" (commandline -ct)(complete -C (commandline -ct))
end
