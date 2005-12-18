#
# Load common ssh options
#

complete -y ssh

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

(__fish_print_users)@

"
complete -c scp -s B -d "Batch mode"
complete -c scp -s l -x -d "Bandwidth limit"
complete -c scp -s P -x -d "Port"
complete -c scp -s p -d "Preserves modification times, access times, and modes from the original file"
complete -c scp -s q -d "Disable progress meter"
complete -c scp -s r -d "Recursively copy"
complete -c scp -s S -d "Encyption program"

