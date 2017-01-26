#
# Load common ssh options
#

__fish_complete_ssh scp

#
# scp specific completions
#

#
# Hostname
#
complete \
	--command     scp \
	--description Hostname \
	--condition   "commandline --cut-at-cursor --current-token | string match --invert '*:*'" \
	--arguments   "

(__fish_print_hostnames):

(
	#Prepend any username specified in the completion to the hostname
	commandline -ct |sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames):

# Disable as username completion is not very useful
# (__fish_print_users)@\tUsername

"

#
# Local path
#
complete \
	--command     scp \
	--description "Local Path" \
	--condition   "commandline -ct | string match ':'"

#
# Remote path
#
complete \
	--command     scp \
	--description "Remote Path" \
	--no-files \
	--condition   "commandline --cut-at-cursor --current-token | string match --regex '.+:'" \
	--arguments   "

(
	#Prepend any user@host information supplied before the remote completion
	commandline -ct| __fish_sgrep -o '.*:'
)(
	#Get the list of remote files from the specified ssh server
        ssh (commandline -c| __fish_sgrep -o '\-P [0-9]*'|tr P p) -o \"BatchMode yes\" (commandline -ct|sed -ne 's/\(.*\):.*/\1/p') ls\ -dp\ (commandline -ct|sed -ne 's/.*://p')\* 2> /dev/null
)

"

complete -c scp -s B --description "Batch mode"
complete -c scp -s l -x --description "Bandwidth limit"
complete -c scp -s P -x --description "Port"
complete -c scp -s p --description "Preserves modification times, access times, and modes from the original file"
complete -c scp -s q --description "Do not display progress bar"
complete -c scp -s r --description "Recursively copy"
complete -c scp -s S --description "Encryption program"
