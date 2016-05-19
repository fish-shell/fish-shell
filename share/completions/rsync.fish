
complete -c rsync -s v -l verbose --description "Increase verbosity"
complete -c rsync -s q -l quiet --description "Suppress non-error messages"
complete -c rsync -s c -l checksum --description "Skip based on checksum, not mod-time & size"
complete -c rsync -s a -l archive --description "Archive mode; same as -rlptgoD (no -H)"
complete -c rsync -l no-OPTION --description "Turn off an implied OPTION (e.g. --no-D)"
complete -c rsync -s r -l recursive --description "Recurse into directories"
complete -c rsync -s R -l relative --description "Use relative path names"
complete -c rsync -l no-implied-dirs --description "Don’t send implied dirs with --relative"
complete -c rsync -s b -l backup --description "Make backups (see --suffix & --backup-dir)"
complete -c rsync -l backup-dir --description "Make backups into hierarchy based in DIR"
complete -c rsync -l suffix --description "Backup suffix (default ~ w/o --backup-dir)"
complete -c rsync -s u -l update --description "Skip files that are newer on the receiver"
complete -c rsync -l inplace --description "Update destination files in-place"
complete -c rsync -l append --description "Append data onto shorter files"
complete -c rsync -s d -l dirs --description "Transfer directories without recursing"
complete -c rsync -s l -l links --description "Copy symlinks as symlinks"
complete -c rsync -s L -l copy-links --description "Transform symlink into referent file/dir"
complete -c rsync -l copy-unsafe-links --description "Only \"unsafe\" symlinks are transformed"
complete -c rsync -l safe-links --description "Ignore symlinks that point outside the tree"
complete -c rsync -s k -l copy-dirlinks --description "Transform symlink to dir into referent dir"
complete -c rsync -s K -l keep-dirlinks --description "Treat symlinked dir on receiver as dir"
complete -c rsync -s H -l hard-links --description "Preserve hard links"
complete -c rsync -s p -l perms --description "Preserve permissions"
complete -c rsync -s E -l executability --description "Preserve executability"
complete -c rsync -s A -l acls --description "Preserve ACLs (implies -p) [non-standard]"
complete -c rsync -s X -l xattrs --description "Preserve extended attrs (implies -p) [n.s.]"
complete -c rsync -l chmod --description "Change destination permissions"
complete -c rsync -s o -l owner --description "Preserve owner (super-user only)"
complete -c rsync -s g -l group --description "Preserve group"
complete -c rsync -l devices --description "Preserve device files (super-user only)"
complete -c rsync -l specials --description "Preserve special files"
complete -c rsync -s D --description "Same as --devices --specials"
complete -c rsync -s t -l times --description "Preserve times"
complete -c rsync -s O -l omit-dir-times --description "Omit directories when preserving times"
complete -c rsync -l super --description "Receiver attempts super-user activities"
complete -c rsync -s S -l sparse --description "Handle sparse files efficiently"
complete -c rsync -s n -l dry-run --description "Show what would have been transferred"
complete -c rsync -s W -l whole-file --description "Copy files whole (without rsync algorithm)"
complete -c rsync -s x -l one-file-system --description "Don’t cross filesystem boundaries"
complete -c rsync -s B -l block-size -x --description "Force a fixed checksum block-size"
complete -c rsync -s e -l rsh -x --description "Specify the remote shell to use"
complete -c rsync -l rsync-path -x --description "Specify the rsync to run on remote machine"
complete -c rsync -l existing --description "Ignore non-existing files on receiving side"
complete -c rsync -l ignore-existing --description "Ignore files that already exist on receiver"
complete -c rsync -l remove-sent-files --description "Sent files/symlinks are removed from sender"
complete -c rsync -l del --description "An alias for --delete-during"
complete -c rsync -l delete --description "Delete files that don’t exist on sender"
complete -c rsync -l delete-before --description "Receiver deletes before transfer (default)"
complete -c rsync -l delete-during --description "Receiver deletes during xfer, not before"
complete -c rsync -l delete-after --description "Receiver deletes after transfer, not before"
complete -c rsync -l delete-excluded --description "Also delete excluded files on receiver"
complete -c rsync -l ignore-errors --description "Delete even if there are I/O errors"
complete -c rsync -l force --description "Force deletion of dirs even if not empty"
complete -c rsync -l max-delete=NUM --description "Don’t delete more than NUM files"
complete -c rsync -l max-size=SIZE --description "Don’t transfer any file larger than SIZE"
complete -c rsync -l min-size=SIZE --description "Don’t transfer any file smaller than SIZE"
complete -c rsync -l partial --description "Keep partially transferred files"
complete -c rsync -l partial-dir=DIR --description "Put a partially transferred file into DIR"
complete -c rsync -l delay-updates --description "Put all updated files into place at end"
complete -c rsync -s m -l prune-empty-dirs --description "Prune empty directory chains from file-list"
complete -c rsync -l numeric-ids --description "Don’t map uid/gid values by user/group name"
complete -c rsync -l timeout --description "Set I/O timeout in seconds"
complete -c rsync -s I -l ignore-times --description "Don’t skip files that match size and time"
complete -c rsync -l size-only --description "Skip files that match in size"
complete -c rsync -l modify-window=NUM --description "Compare mod-times with reduced accuracy"
complete -c rsync -s T -l temp-dir=DIR --description "Create temporary files in directory DIR"
complete -c rsync -s y -l fuzzy --description "Find similar file for basis if no dest file"
complete -c rsync -l compare-dest -x --description "Also compare received files relative to DIR"
complete -c rsync -l copy-dest -x --description "Also compare received files relative to DIR and include copies of unchanged files"
complete -c rsync -l link-dest=DIR --description "Hardlink to files in DIR when unchanged"
complete -c rsync -s z -l compress --description "Compress file data during the transfer"
complete -c rsync -l compress-level --description "Explicitly set compression level"
complete -c rsync -s C -l cvs-exclude --description "Auto-ignore files in the same way CVS does"
complete -c rsync -s f -l filter --description "Add a file-filtering RULE"
complete -c rsync -s F --description "Same as --filter=’dir-merge /.rsync-filter’ repeated: --filter='- .rsync-filter'"
complete -c rsync -l exclude --description "Exclude files matching PATTERN"
complete -c rsync -l exclude-from --description "Read exclude patterns from FILE"
complete -c rsync -l include --description "Don’t exclude files matching PATTERN"
complete -c rsync -l include-from=FILE --description "Read include patterns from FILE"
complete -c rsync -l files-from=FILE --description "Read list of source-file names from FILE"
complete -c rsync -s 0 -l from0 --description "All *from/filter files are delimited by 0s"
complete -c rsync -l address --description "Bind address for outgoing socket to daemon"
complete -c rsync -l port --description "Specify double-colon alternate port number"
complete -c rsync -l sockopts --description "Specify custom TCP options"
complete -c rsync -l blocking-io --description "Use blocking I/O for the remote shell"
complete -c rsync -l stats --description "Give some file-transfer stats"
complete -c rsync -s 8 -l 8-bit-output --description "Leave high-bit chars unescaped in output"
complete -c rsync -s h -l human-readable --description "Output numbers in a human-readable format"
complete -c rsync -l progress --description "Show progress during transfer"
complete -c rsync -s P --description "Same as --partial --progress"
complete -c rsync -s i -l itemize-changes --description "Output a change-summary for all updates"
complete -c rsync -l log-format -x --description "Output filenames using the specified format"
complete -c rsync -l password-file -x --description "Read password from FILE"
complete -c rsync -l list-only --description "List the files instead of copying them"
complete -c rsync -l bwlimit -x --description "Limit I/O bandwidth; KBytes per second"
complete -c rsync -l write-batch -x --description "Write a batched update to FILE"
complete -c rsync -l only-write-batch --description "Like --write-batch but w/o updating dest"
complete -c rsync -l read-batch -x --description "Read a batched update from FILE"
complete -c rsync -l protocol -x --description "Force an older protocol version to be used"
complete -c rsync -l checksum-seed -x --description "Set block/file checksum seed (advanced)"
complete -c rsync -s 4 -l ipv4 --description "Prefer IPv4"
complete -c rsync -s 6 -l ipv6 --description "Prefer IPv6"
complete -c rsync -l version --description "Display version and exit"
complete -c rsync -l help --description "Display help and exit"

#
# Hostname completion
#
complete -c rsync -d Hostname -a "

(__fish_print_hostnames):

(
	#Prepend any username specified in the completion to the hostname
	commandline -ct |sed -ne 's/\(.*@\).*/\1/p'
)(__fish_print_hostnames):

(__fish_print_users)@\tUsername

"

#
# Remote path
#
complete -c rsync -d "Remote path" -n "commandline -ct| __fish_sgrep -q :" -a "
(
	#Prepend any user@host:/path information supplied before the remote completion
	commandline -ct| __fish_sgrep -Eo '.*:+(.*/)?'
)(
	#Get the list of remote files from the specified rsync server
	rsync --list-only (commandline -ct| __fish_sgrep -Eo '.*:+(.*/)?') ^/dev/null | sed '/^d/ s,\$,/, ' | tr -s ' '| cut -d' ' -f 5-
)
"
