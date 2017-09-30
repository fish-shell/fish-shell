#
# Load completions shared by ssh and scp.
#
__fish_complete_ssh scp

# Helper functions to simplify the completions.
function __scp2ssh_port_number
    # There is a silly inconsistency between the ssh and scp commands regarding the short flag name
    # for specifying the TCP port number. This function deals with that by extracting the port
    # number if present and emitting it as a flag appropriate for ssh.
    set -l port (commandline -c | string match -r -- ' -P ?(\d+)\b')
    and echo -p\n$port[2]
end

function __scp_remote_target
    set -l target (commandline -ct | string match -r -- '(.*):')
    and echo $target[2]
end

function __scp_remote_path_prefix
    set -l path_prefix (commandline -ct | string match -r -- ':(.*)')
    and echo $path_prefix[2]
end

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
(__scp_remote_target):(
	# Get the list of remote files from the scp target.
        ssh (__scp2ssh_port_number) -o 'BatchMode yes' (__scp_remote_target) ls\ -dp\ (__scp_remote_path_prefix)\* 2>/dev/null
)
"
complete -c scp -s B -d "Batch mode"
complete -c scp -s l -x -d "Bandwidth limit"
complete -c scp -s P -x -d "Port"
complete -c scp -s p -d "Preserves modification times, access times, and modes from the original file"
complete -c scp -s q -d "Do not display progress bar"
complete -c scp -s r -d "Recursively copy"
complete -c scp -s S -d "Encryption program"
