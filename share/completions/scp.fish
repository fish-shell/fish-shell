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
	echo (commandline -ct)|sed -ne 's/\(.*@\).*/\1/p'
)(
	cat ~/.ssh/known_hosts{,2} ^/dev/null|cut -d ' ' -f 1| cut -d , -f 1
):

(__fish_print_users)@\tUsername

"

#
# Remote path
#
complete -c scp -d "Remote Path" -n "echo (commandline -ct)|sgrep -o '.*:';and true" -a "

(
	#Prepend any user@host information supplied before the remote completion
	echo (commandline -ct)|sgrep -o '.*:'
)(
	#Get the list of remote files from the specified ssh server
        ssh -o \"BatchMode yes\" (echo (commandline -ct)|sed -ne 's/\(.*\):.*/\1/p') ls\ -dp\ (echo (commandline -ct)|sed -ne 's/.*://p')\*
)

"

complete -c scp -s B --description "Batch mode"
complete -c scp -s l -x --description "Bandwidth limit"
complete -c scp -s P -x --description "Port"
complete -c scp -s p --description "Preserves modification times, access times, and modes from the original file"
complete -c scp -s q --description "Do not display progress bar"
complete -c scp -s r --description "Recursively copy"
complete -c scp -s S --description "Encryption program"

