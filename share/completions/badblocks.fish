#
# Command specific completions for the badblocks command.
# These completions where generated from the commands
# man page by the make_completions.py script, and
# have been hand edited since.
#

# magic completion safety check (do not remove this comment)
if not type -q badblocks
    exit
end

complete -c badblocks -s b -d 'Block-size Specify the size of blocks in bytes'
complete -c badblocks -s c -d 'Number of blocks is the number of blocks which are tested at a time'
complete -c badblocks -s f -d 'Normally, badblocks will refuse to do a read/write or a nondestructive test on a device which is mounted, since either can cause the system to potentially crash and/or damage the filesystem even if it is mounted read-only'
complete -c badblocks -s i -d 'Input_file Read a list of already existing known bad blocks'
complete -c badblocks -s o -d 'Output_file Write the list of bad blocks to the specified file'
complete -c badblocks -s p -d 'Repeat scanning the disk until there are no new blocks discovered in specified number of consecutive scans of the disk'
complete -c badblocks -s t -d 'Test_pattern Specify a test pattern to be read (and written) to disk blocks'
complete -c badblocks -s n -d 'Use non-destructive read-write mode'
complete -c badblocks -s s -d 'Show the progress of the scan by writing out the block numbers as they are checked'
complete -c badblocks -s v -d 'Verbose mode'
complete -c badblocks -s w -d 'Use write-mode test'
complete -c badblocks -s X -d 'Internal flag only to be used by e2fsck(8) and mke2fs(8)'
