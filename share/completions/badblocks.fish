#
# Command specific completions for the badblocks command.
# These completions where generated from the commands
# man page by the make_completions.py script, and
# have been hand edited since.
#

complete -c badblocks -s b -d 'Block-size Specify the size of blocks in bytes'
complete -c badblocks -s c -d 'Number of blocks is the number of blocks which are tested at a time'
complete -c badblocks -s f -d 'Execute test even when device is mounted (dangerous!)'
complete -c badblocks -s i -d 'Input_file Read a list of already existing known bad blocks'
complete -c badblocks -s o -d 'Output_file Write the list of bad blocks to the specified file'
complete -c badblocks -s p -d 'Repeat until no new blocks are found in provided number of scans'
complete -c badblocks -s t -d 'Test_pattern Specify a test pattern to be read (and written) to disk blocks'
complete -c badblocks -s n -d 'Use non-destructive read-write mode'
complete -c badblocks -s s -d 'Show scan progress'
complete -c badblocks -s v -d 'Verbose mode'
complete -c badblocks -s w -d 'Use write-mode test'
complete -c badblocks -s X -d 'Internal flag only to be used by e2fsck(8) and mke2fs(8)'
