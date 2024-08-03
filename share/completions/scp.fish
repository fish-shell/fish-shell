# Load completions shared by various ssh tools like ssh, scp and sftp.
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

function __fish_no_scp_remote_specified
    set -l tokens (commandline -t)
    # can't use `for token in tokens[1..-2]` due to https://github.com/fish-shell/fish-shell/issues/4897
    set -e tokens[-1]
    for token in $tokens # ignoring current token
        if string match -e @ -- $token
            return 1
        end
    end
    return 0
end

#
# scp specific completions
#

# Inherit user/host completions from ssh
complete -c scp -d Remote -n "__fish_no_scp_remote_specified; and not string match -e -- : (commandline -ct)" -a "(complete -C'ssh ' | string replace -r '\t.*' ':')"

# Local path
complete -c scp -d "Local Path" -n "not string match @ -- (commandline -ct)"

# Remote path
# Get the list of remote files from the scp target.
if string match -rq 'OpenSSH(_for_Windows)?_(?<major>\d+)\.*' -- (ssh -V 2>&1) && test "$major" -ge 9
    complete -c scp -d "Remote Path" -f -n "commandline -ct | string match -e ':'" -a "
    (__scp_remote_target):( \
            command ssh (__scp2ssh_port_number) -o 'BatchMode yes' (__scp_remote_target) command\ ls\ -dp\ (__scp_remote_path_prefix)\* 2>/dev/null
    )
    "
else
    complete -c scp -d "Remote Path" -f -n "commandline -ct | string match -e ':'" -a "
    (__scp_remote_target):( \
            command ssh (__scp2ssh_port_number) -o 'BatchMode yes' (__scp_remote_target) command\ ls\ -dp\ (__scp_remote_path_prefix | string unescape)\* 2>/dev/null |
            string escape -n
    )
    "
end

complete -c scp -s 3 -d "Copies between two remote hosts are transferred through the local host"
complete -c scp -s B -d "Batch mode"
complete -c scp -s D -x -d "Connect directly to a local SFTP server"
complete -c scp -s l -x -d "Bandwidth limit"
complete -c scp -s O -d "Use original SCP protocol instead of SFTP"
complete -c scp -s P -x -d Port
complete -c scp -s p -d "Preserves modification times, access times, and modes from the original file"
complete -c scp -s R -d "Copies between two remote hosts are performed by executing scp on the origin host"
complete -c scp -s r -d "Recursively copy"
complete -c scp -s S -d "Encryption program"
complete -c scp -s T -d "Disable strict filename checking"
