# Load completions shared by various ssh tools like ssh, scp and sftp.
__fish_complete_ssh sftp

complete -c sftp -x -a "(__fish_complete_user_at_hosts)"

complete -c sftp -s a -d 'Attempt to continue interrupted transfers'
complete -c sftp -s B -x -d 'Size of the buffer when transferring files'
complete -c sftp -s b -F -d 'Reads a series of commands from an input batchfile'
complete -c sftp -s D -x -d 'Connect directly to a local SFTP server'
complete -c sftp -s f -d 'Flush files to disk after transfer'
complete -c sftp -s l -x -d 'Limits the used bandwidth (Kbit/s)'
complete -c sftp -s N -d 'Disables quiet mode'
complete -c sftp -s P -x -d 'Port to connect to on the remote host'
complete -c sftp -s p -d 'Preserve timestamps from the original files transferred'
complete -c sftp -s R -x -d 'How many requests may be outstanding'
complete -c sftp -s r -d 'Recursively copy entire directories'
complete -c sftp -s S -r -d 'Program to use for the encrypted connection'
complete -c sftp -s s -x -d 'The SSH2 subsystem or the path for an sftp server'
