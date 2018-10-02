function __rsync_remote_target
    commandline -ct | string match -r '.*::?(?:.*/)?'
end

complete -c rsync -s v -l verbose -d "Increase verbosity"
complete -c rsync -s q -l quiet -d "Suppress non-error messages"
complete -c rsync -s c -l checksum -d "Skip based on checksum, not mod-time & size"
complete -c rsync -s a -l archive -d "Archive mode; same as -rlptgoD (no -H)"
complete -c rsync -l no-OPTION -d "Turn off an implied OPTION (e.g. --no-D)"
complete -c rsync -s r -l recursive -d "Recurse into directories"
complete -c rsync -s R -l relative -d "Use relative path names"
complete -c rsync -l no-implied-dirs -d "Don’t send implied dirs with --relative"
complete -c rsync -s b -l backup -d "Make backups (see --suffix & --backup-dir)"
complete -c rsync -l backup-dir -d "Make backups into hierarchy based in DIR"
complete -c rsync -l suffix -d "Backup suffix (default ~ w/o --backup-dir)"
complete -c rsync -s u -l update -d "Skip files that are newer on the receiver"
complete -c rsync -l inplace -d "Update destination files in-place"
complete -c rsync -l append -d "Append data onto shorter files"
complete -c rsync -s d -l dirs -d "Transfer directories without recursing"
complete -c rsync -s l -l links -d "Copy symlinks as symlinks"
complete -c rsync -s L -l copy-links -d "Transform symlink into referent file/dir"
complete -c rsync -l copy-unsafe-links -d "Only \"unsafe\" symlinks are transformed"
complete -c rsync -l safe-links -d "Ignore symlinks that point outside the tree"
complete -c rsync -s k -l copy-dirlinks -d "Transform symlink to dir into referent dir"
complete -c rsync -s K -l keep-dirlinks -d "Treat symlinked dir on receiver as dir"
complete -c rsync -s H -l hard-links -d "Preserve hard links"
complete -c rsync -s p -l perms -d "Preserve permissions"
complete -c rsync -s E -l executability -d "Preserve executability"
complete -c rsync -s A -l acls -d "Preserve ACLs (implies -p) [non-standard]"
complete -c rsync -s X -l xattrs -d "Preserve extended attrs (implies -p) [n.s.]"
complete -c rsync -l chmod -d "Change destination permissions"
complete -c rsync -s o -l owner -d "Preserve owner (super-user only)"
complete -c rsync -s g -l group -d "Preserve group"
complete -c rsync -l devices -d "Preserve device files (super-user only)"
complete -c rsync -l specials -d "Preserve special files"
complete -c rsync -s D -d "Same as --devices --specials"
complete -c rsync -s t -l times -d "Preserve times"
complete -c rsync -s O -l omit-dir-times -d "Omit directories when preserving times"
complete -c rsync -l super -d "Receiver attempts super-user activities"
complete -c rsync -s S -l sparse -d "Handle sparse files efficiently"
complete -c rsync -s n -l dry-run -d "Show what would have been transferred"
complete -c rsync -s W -l whole-file -d "Copy files whole (without rsync algorithm)"
complete -c rsync -s x -l one-file-system -d "Don’t cross filesystem boundaries"
complete -c rsync -s B -l block-size -x -d "Force a fixed checksum block-size"
complete -c rsync -s e -l rsh -x -d "Specify the remote shell to use"
complete -c rsync -l rsync-path -x -d "Specify the rsync to run on remote machine"
complete -c rsync -l existing -d "Ignore non-existing files on receiving side"
complete -c rsync -l ignore-existing -d "Ignore files that already exist on receiver"
complete -c rsync -l remove-sent-files -d "Sent files/symlinks are removed from sender"
complete -c rsync -l remove-source-files -d "Remove all synced files from source/sender"
complete -c rsync -l del -d "An alias for --delete-during"
complete -c rsync -l delete -d "Delete files that don’t exist on sender"
complete -c rsync -l delete-before -d "Receiver deletes before transfer (default)"
complete -c rsync -l delete-during -d "Receiver deletes during xfer, not before"
complete -c rsync -l delete-after -d "Receiver deletes after transfer, not before"
complete -c rsync -l delete-excluded -d "Also delete excluded files on receiver"
complete -c rsync -l ignore-errors -d "Delete even if there are I/O errors"
complete -c rsync -l force -d "Force deletion of dirs even if not empty"
complete -c rsync -l max-delete=NUM -d "Don’t delete more than NUM files"
complete -c rsync -l max-size=SIZE -d "Don’t transfer any file larger than SIZE"
complete -c rsync -l min-size=SIZE -d "Don’t transfer any file smaller than SIZE"
complete -c rsync -l partial -d "Keep partially transferred files"
complete -c rsync -l partial-dir=DIR -d "Put a partially transferred file into DIR"
complete -c rsync -l delay-updates -d "Put all updated files into place at end"
complete -c rsync -s m -l prune-empty-dirs -d "Prune empty directory chains from file-list"
complete -c rsync -l numeric-ids -d "Don’t map uid/gid values by user/group name"
complete -c rsync -l timeout -d "Set I/O timeout in seconds"
complete -c rsync -s I -l ignore-times -d "Don’t skip files that match size and time"
complete -c rsync -l size-only -d "Skip files that match in size"
complete -c rsync -l modify-window=NUM -d "Compare mod-times with reduced accuracy"
complete -c rsync -s T -l temp-dir=DIR -d "Create temporary files in directory DIR"
complete -c rsync -s y -l fuzzy -d "Find similar file for basis if no dest file"
complete -c rsync -l compare-dest -x -d "Also compare received files relative to DIR"
complete -c rsync -l copy-dest -x -d "Also compare received files relative to DIR and include copies of unchanged files"
complete -c rsync -l link-dest=DIR -d "Hardlink to files in DIR when unchanged"
complete -c rsync -s z -l compress -d "Compress file data during the transfer"
complete -c rsync -l compress-level -d "Explicitly set compression level"
complete -c rsync -s C -l cvs-exclude -d "Auto-ignore files in the same way CVS does"
complete -c rsync -s f -l filter -d "Add a file-filtering RULE"
complete -c rsync -s F -d "Same as --filter=’dir-merge /.rsync-filter’ repeated: --filter='- .rsync-filter'"
complete -c rsync -l exclude -d "Exclude files matching PATTERN"
complete -c rsync -l exclude-from -d "Read exclude patterns from FILE"
complete -c rsync -l include -d "Don’t exclude files matching PATTERN"
complete -c rsync -l include-from=FILE -d "Read include patterns from FILE"
complete -c rsync -l files-from=FILE -d "Read list of source-file names from FILE"
complete -c rsync -s 0 -l from0 -d "All *from/filter files are delimited by 0s"
complete -c rsync -l address -d "Bind address for outgoing socket to daemon"
complete -c rsync -l port -d "Specify double-colon alternate port number"
complete -c rsync -l sockopts -d "Specify custom TCP options"
complete -c rsync -l blocking-io -d "Use blocking I/O for the remote shell"
complete -c rsync -l stats -d "Give some file-transfer stats"
complete -c rsync -s 8 -l 8-bit-output -d "Leave high-bit chars unescaped in output"
complete -c rsync -s h -l human-readable -d "Output numbers in a human-readable format"
complete -c rsync -l progress -d "Show progress during transfer"
complete -c rsync -s P -d "Same as --partial --progress"
complete -c rsync -s i -l itemize-changes -d "Output a change-summary for all updates"
complete -c rsync -l log-format -x -d "Output filenames using the specified format"
complete -c rsync -l password-file -x -d "Read password from FILE"
complete -c rsync -l list-only -d "List the files instead of copying them"
complete -c rsync -l bwlimit -x -d "Limit I/O bandwidth; KBytes per second"
complete -c rsync -l write-batch -x -d "Write a batched update to FILE"
complete -c rsync -l only-write-batch -d "Like --write-batch but w/o updating dest"
complete -c rsync -l read-batch -x -d "Read a batched update from FILE"
complete -c rsync -l protocol -x -d "Force an older protocol version to be used"
complete -c rsync -l checksum-seed -x -d "Set block/file checksum seed (advanced)"
complete -c rsync -s 4 -l ipv4 -d "Prefer IPv4"
complete -c rsync -s 6 -l ipv6 -d "Prefer IPv6"
complete -c rsync -l version -d "Display version and exit"
complete -c rsync -l help -d "Display help and exit"

#
# Hostname completion
#
complete -c rsync -d Hostname -a "
(__fish_print_hostnames):

(
	# Prepend any username specified in the completion to the hostname.
	commandline -ct |sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames):

(__fish_print_users)@\tUsername
"

#
# Remote path
#
complete -c rsync -d "Remote path" -n "commandline -ct | string match -q '*:*'" -a "
(
	# Prepend any user@host:/path information supplied before the remote completion.
        __rsync_remote_target
)(
	# Get the list of remote files from the specified rsync server.
        rsync --list-only (__rsync_remote_target) 2>/dev/null | string replace -r '^d.*' '\$0/' | tr -s ' ' | cut -d' ' -f 5-
)
"
