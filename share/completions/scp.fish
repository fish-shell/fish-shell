#
# Load common ssh options
#

__fish_complete_ssh scp

#
# scp specific completions
#

complete -c scp -d Hostname -a "

(
	#Find a suitable hostname from the knownhosts files
	cat ~/.ssh/known_hosts{,2} ^/dev/null|cut -d ' ' -f 1| cut -d , -f 1
):

(
	#Prepend any username specified in the completion to the hostname
	commandline -ct |sed -ne 's/\(.*@\).*/\1/p'
)(
	cat ~/.ssh/known_hosts{,2} ^/dev/null|cut -d ' ' -f 1| cut -d , -f 1
):

(__fish_print_users)@\tUsername

"

#
# Remote path
#
complete -c scp -d "Remote Path" -n "commandline -ct|sgrep -o '.*:'" -a "

(
	#Prepend any user@host information supplied before the remote completion
	commandline -ct|sgrep -o '.*:'
)(
	#Get the list of remote files from the specified ssh server
        ssh -o \"BatchMode yes\" (commandline -ct|sed -ne 's/\(.*\):.*/\1/p') ls\ -dp\ (commandline -ct|sed -ne 's/.*://p')\* 2> /dev/null
)

"

complete -c scp -s B --description "Batch mode"
complete -c scp -s l -x --description "Bandwidth limit"
complete -c scp -s P -x --description "Port"
complete -c scp -s p --description "Preserves modification times, access times, and modes from the original file"
complete -c scp -s q --description "Do not display progress bar"
complete -c scp -s r --description "Recursively copy"
complete -c scp -s S --description "Encryption program"

