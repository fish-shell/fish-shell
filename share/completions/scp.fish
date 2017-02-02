#
# Load completions shared by ssh and scp.
#
__fish_complete_ssh scp

#
# scp specific completions
#

#
# Hostname
#
complete -c scp -d Hostname -n "commandline --cut-at-cursor --current-token | string match -v '*:*'" -a "

(__fish_print_hostnames):

(
	#Prepend any username specified in the completion to the hostname
	commandline -ct |sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames):
"

#
# Local path
#
complete -c scp -d "Local Path" -n "commandline -ct | string match ':'"

#
# Remote path
#
complete -c scp -d "Remote Path" -f -n "commandline --cut-at-cursor --current-token | string match -r '.+:'" -a "

(
	#Prepend any user@host information supplied before the remote completion
	commandline -ct| __fish_sgrep -o '.*:'
)(
	#Get the list of remote files from the specified ssh server
        ssh (commandline -c| __fish_sgrep -o '\-P [0-9]*'|tr P p) -o \"BatchMode yes\" (commandline -ct|sed -ne 's/\(.*\):.*/\1/p') ls\ -dp\ (commandline -ct|sed -ne 's/.*://p')\* 2> /dev/null
)

"
complete -c scp -s B -d "Batch mode"
complete -c scp -s l -x -d "Bandwidth limit"
complete -c scp -s P -x -d "Port"
complete -c scp -s p -d "Preserves modification times, access times, and modes from the original file"
complete -c scp -s q -d "Do not display progress bar"
complete -c scp -s r -d "Recursively copy"
complete -c scp -s S -d "Encryption program"
